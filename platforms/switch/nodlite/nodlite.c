// nodlite: a small C implementation of the parts of nod's C API (nod.h, v2.0.0-alpha.12) that
// Aurora's DVD layer uses, for platforms where nod's Rust library cannot be built (Horizon).
//
// GameCube discs only, from plain ISO/GCM, CISO and GCZ images. Wii discs, RVZ/WIA/WBFS/NFS/TGC
// return NOD_RESULT_ERR_FORMAT; convert those to ISO in Dolphin.

#include "nod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#if defined(__SWITCH__)
#include <switch.h>
#define NL_MUTEX Mutex
#define NL_MUTEX_INIT(m) mutexInit(m)
#define NL_LOCK(m) mutexLock(m)
#define NL_UNLOCK(m) mutexUnlock(m)
#else
#include <pthread.h>
#define NL_MUTEX pthread_mutex_t
#define NL_MUTEX_INIT(m) pthread_mutex_init(m, NULL)
#define NL_LOCK(m) pthread_mutex_lock(m)
#define NL_UNLOCK(m) pthread_mutex_unlock(m)
#endif

enum { NL_DISC = 1, NL_PARTITION = 2, NL_FILE = 3 };

typedef struct NlImage {
  NodDiscStream stream;
  NodFormat format;
  uint64_t size;       // decompressed disc size
  uint32_t blockSize;  // CISO/GCZ
  uint32_t numBlocks;
  // CISO: physical block index per logical block, or UINT32_MAX when absent (zeroes).
  uint32_t* cisoMap;
  // GCZ: block pointers (bit 63 = stored uncompressed) and the data start.
  uint64_t* gczPtrs;
  uint64_t gczDataStart;
  uint64_t gczCompressedSize;
  // One decompressed block cached, guarded by a lock (DVD reads run on a worker thread).
  uint8_t* cache;
  uint8_t* scratch;
  int64_t cachedBlock;
  NL_MUTEX lock;
  int refs;
} NlImage;

typedef struct NlFstNode {
  uint32_t isDir;
  uint32_t nameOff;
  uint32_t offset;  // file: disc offset; dir: parent
  uint32_t length;  // file: size; dir: next index
} NlFstNode;

struct NodHandle {
  int kind;
  NlImage* img;
  uint64_t base;  // window start on the disc
  uint64_t len;   // window length
  uint64_t pos;   // position inside the window
  // nod_buf_read's buffer
  uint8_t* buf;
  uint64_t bufStart;
  size_t bufLen;
  // Partition only
  NodHandle* parent;
  uint8_t* boot;
  uint8_t* bi2;
  uint8_t* apploader;
  size_t apploaderSize;
  uint8_t* dol;
  size_t dolSize;
  uint8_t* fst;
  size_t fstSize;
  NlFstNode* nodes;
  uint32_t nodeCount;
  const char* strings;
  size_t stringsSize;
};

static _Thread_local char s_error[256];

static void set_error(const char* msg) {
  snprintf(s_error, sizeof s_error, "%s", msg);
}

const char* nod_error_message(void) {
  return s_error[0] ? s_error : NULL;
}

static uint32_t be32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static uint32_t le32(const uint8_t* p) {
  return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}
static uint64_t le64(const uint8_t* p) {
  return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static int raw_read(NlImage* img, uint64_t off, void* out, size_t len) {
  uint8_t* dst = out;
  while (len > 0) {
    const int64_t n = img->stream.read_at(img->stream.user_data, off, dst, len);
    if (n <= 0) {
      return -1;
    }
    dst += n;
    off += (uint64_t)n;
    len -= (size_t)n;
  }
  return 0;
}

// Loads logical block `b` into img->cache. Caller holds the lock.
static int load_block(NlImage* img, uint32_t b) {
  if (img->cachedBlock == (int64_t)b) {
    return 0;
  }
  img->cachedBlock = -1;
  if (img->format == NOD_FORMAT_CISO) {
    if (img->cisoMap[b] == UINT32_MAX) {
      memset(img->cache, 0, img->blockSize);
    } else if (raw_read(img, 0x8000ull + (uint64_t)img->cisoMap[b] * img->blockSize, img->cache, img->blockSize) != 0) {
      return -1;
    }
  } else {
    const uint64_t ptr = img->gczPtrs[b];
    const int stored = (ptr >> 63) != 0;
    const uint64_t start = ptr & ~(1ull << 63);
    uint64_t end;
    if (b + 1 < img->numBlocks) {
      end = img->gczPtrs[b + 1] & ~(1ull << 63);
    } else {
      end = img->gczCompressedSize;
    }
    if (end < start || end - start > (uint64_t)img->blockSize * 2 + 64) {
      return -1;
    }
    const size_t csize = (size_t)(end - start);
    if (stored) {
      memset(img->cache, 0, img->blockSize);
      if (raw_read(img, img->gczDataStart + start, img->cache, csize < img->blockSize ? csize : img->blockSize) != 0) {
        return -1;
      }
    } else {
      if (raw_read(img, img->gczDataStart + start, img->scratch, csize) != 0) {
        return -1;
      }
      uLongf outLen = img->blockSize;
      const int zr = uncompress(img->cache, &outLen, img->scratch, (uLong)csize);
      if (zr != Z_OK && zr != Z_BUF_ERROR) {
        return -1;
      }
      if (outLen < img->blockSize) {
        memset(img->cache + outLen, 0, img->blockSize - outLen);
      }
    }
  }
  img->cachedBlock = b;
  return 0;
}

// Reads decompressed disc bytes.
static int disc_read(NlImage* img, uint64_t off, void* out, size_t len) {
  if (off >= img->size) {
    return len == 0 ? 0 : -1;
  }
  if (len > img->size - off) {
    return -1;
  }
  if (img->format == NOD_FORMAT_ISO) {
    return raw_read(img, off, out, len);
  }
  uint8_t* dst = out;
  int rc = 0;
  NL_LOCK(&img->lock);
  while (len > 0) {
    const uint32_t b = (uint32_t)(off / img->blockSize);
    const uint32_t inBlock = (uint32_t)(off % img->blockSize);
    size_t n = img->blockSize - inBlock;
    if (n > len) {
      n = len;
    }
    if (b >= img->numBlocks) {
      memset(dst, 0, n);
    } else if (img->format == NOD_FORMAT_CISO && img->cisoMap[b] != UINT32_MAX && inBlock == 0 && n == img->blockSize) {
      // Whole present block: read straight into the destination.
      NL_UNLOCK(&img->lock);
      rc = raw_read(img, 0x8000ull + (uint64_t)img->cisoMap[b] * img->blockSize, dst, n);
      NL_LOCK(&img->lock);
      if (rc != 0) {
        break;
      }
    } else {
      if (load_block(img, b) != 0) {
        rc = -1;
        break;
      }
      memcpy(dst, img->cache + inBlock, n);
    }
    dst += n;
    off += n;
    len -= n;
  }
  NL_UNLOCK(&img->lock);
  return rc;
}

static void image_release(NlImage* img) {
  if (img == NULL || --img->refs > 0) {
    return;
  }
  if (img->stream.close) {
    img->stream.close(img->stream.user_data);
  }
  free(img->cisoMap);
  free(img->gczPtrs);
  free(img->cache);
  free(img->scratch);
  free(img);
}

static NodResult image_open(const NodDiscStream* stream, NlImage** out) {
  NlImage* img = calloc(1, sizeof *img);
  if (img == NULL) {
    return NOD_RESULT_ERR_OTHER;
  }
  img->stream = *stream;
  img->cachedBlock = -1;
  img->refs = 1;
  NL_MUTEX_INIT(&img->lock);
  const int64_t fileLen = stream->stream_len(stream->user_data);
  uint8_t hdr[0x20];
  if (fileLen < 0x440 || raw_read(img, 0, hdr, sizeof hdr) != 0) {
    set_error("disc image too small or unreadable");
    goto fail_io;
  }
  if (memcmp(hdr, "CISO", 4) == 0) {
    img->format = NOD_FORMAT_CISO;
    img->blockSize = le32(hdr + 4);
    if (img->blockSize == 0 || img->blockSize > (16u << 20)) {
      set_error("CISO: bad block size");
      goto fail_fmt;
    }
    uint8_t map[0x8000 - 8];
    if (raw_read(img, 8, map, sizeof map) != 0) {
      goto fail_io;
    }
    uint32_t last = 0;
    for (uint32_t i = 0; i < sizeof map; i++) {
      if (map[i]) {
        last = i + 1;
      }
    }
    img->numBlocks = last;
    img->cisoMap = malloc(sizeof(uint32_t) * (last ? last : 1));
    uint32_t phys = 0;
    for (uint32_t i = 0; i < last; i++) {
      img->cisoMap[i] = map[i] ? phys++ : UINT32_MAX;
    }
    img->size = (uint64_t)last * img->blockSize;
    // CISO drops the trailing empty blocks; a GameCube disc is a full mini-DVD, as nod reports it.
    if (img->size < 1459978240ull) {
      img->size = 1459978240ull;
    }
  } else if (le32(hdr) == 0xB10BC001u) {
    img->format = NOD_FORMAT_GCZ;
    img->gczCompressedSize = le64(hdr + 8);
    img->size = le64(hdr + 16);
    img->blockSize = le32(hdr + 24);
    img->numBlocks = le32(hdr + 28);
    if (img->blockSize == 0 || img->blockSize > (16u << 20) || img->numBlocks == 0) {
      set_error("GCZ: bad header");
      goto fail_fmt;
    }
    img->gczPtrs = malloc(sizeof(uint64_t) * img->numBlocks);
    uint8_t* raw = malloc(8ull * img->numBlocks);
    if (img->gczPtrs == NULL || raw == NULL || raw_read(img, 32, raw, 8ull * img->numBlocks) != 0) {
      free(raw);
      goto fail_io;
    }
    for (uint32_t i = 0; i < img->numBlocks; i++) {
      img->gczPtrs[i] = le64(raw + 8ull * i);
    }
    free(raw);
    img->gczDataStart = 32 + 12ull * img->numBlocks;  // pointers + hashes
    img->scratch = malloc(img->blockSize * 2 + 64);
  } else {
    img->format = NOD_FORMAT_ISO;
    img->size = (uint64_t)fileLen;
  }
  if (img->format != NOD_FORMAT_ISO) {
    img->cache = malloc(img->blockSize);
    if (img->cache == NULL) {
      goto fail_io;
    }
  }
  uint8_t magic[4];
  if (disc_read(img, 0x18, magic, 4) == 0 && NOD_MAGIC_EQ(magic, WII_MAGIC)) {
    set_error("Wii discs are not supported by nodlite");
    goto fail_fmt;
  }
  if (disc_read(img, 0x1C, magic, 4) != 0 || !NOD_MAGIC_EQ(magic, GCN_MAGIC)) {
    set_error("not a GameCube disc image (ISO, CISO or GCZ; convert RVZ/WIA in Dolphin)");
    goto fail_fmt;
  }
  *out = img;
  return NOD_RESULT_OK;
fail_fmt:
  img->stream.close = NULL;
  image_release(img);
  return NOD_RESULT_ERR_FORMAT;
fail_io:
  img->stream.close = NULL;
  image_release(img);
  return NOD_RESULT_ERR_IO;
}

static int64_t file_read_at(void* ud, uint64_t off, void* out, size_t len) {
  FILE* f = ud;
  if (fseeko(f, (off_t)off, SEEK_SET) != 0) {
    return -1;
  }
  const size_t n = fread(out, 1, len, f);
  return n == 0 && ferror(f) ? -1 : (int64_t)n;
}
static int64_t file_len(void* ud) {
  FILE* f = ud;
  if (fseeko(f, 0, SEEK_END) != 0) {
    return -1;
  }
  return (int64_t)ftello(f);
}
static void file_close(void* ud) {
  fclose((FILE*)ud);
}

static NodHandle* handle_new(int kind, NlImage* img, uint64_t base, uint64_t len) {
  NodHandle* h = calloc(1, sizeof *h);
  if (h == NULL) {
    return NULL;
  }
  h->kind = kind;
  h->img = img;
  h->base = base;
  h->len = len;
  img->refs++;
  return h;
}

NodResult nod_disc_open_stream(const NodDiscStream* stream, const NodDiscOptions* options, NodHandle** out) {
  (void)options;
  if (stream == NULL || out == NULL || !stream->read_at || !stream->stream_len || !stream->close) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  NlImage* img;
  const NodResult r = image_open(stream, &img);
  if (r != NOD_RESULT_OK) {
    return r;
  }
  NodHandle* h = handle_new(NL_DISC, img, 0, img->size);
  image_release(img);  // the handle holds the only reference now
  if (h == NULL) {
    return NOD_RESULT_ERR_OTHER;
  }
  *out = h;
  return NOD_RESULT_OK;
}

NodResult nod_disc_open(const char* path, const NodDiscOptions* options, NodHandle** out) {
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    set_error("cannot open disc image");
    return NOD_RESULT_ERR_IO;
  }
  const NodDiscStream s = {f, file_read_at, file_len, file_close};
  const NodResult r = nod_disc_open_stream(&s, options, out);
  if (r != NOD_RESULT_OK) {
    fclose(f);
  }
  return r;
}

static uint8_t* read_blob(NlImage* img, uint64_t off, size_t len) {
  uint8_t* p = malloc(len ? len : 1);
  if (p != NULL && disc_read(img, off, p, len) != 0) {
    free(p);
    return NULL;
  }
  return p;
}

NodResult nod_disc_open_partition_kind(NodHandle* disc, uint32_t kind, const NodPartitionOptions* options,
                                       NodHandle** out) {
  (void)options;
  if (disc == NULL || disc->kind != NL_DISC || out == NULL) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  if (kind != NOD_PARTITION_KIND_DATA) {
    return NOD_RESULT_ERR_NOT_FOUND;
  }
  NlImage* img = disc->img;
  NodHandle* p = handle_new(NL_PARTITION, img, 0, img->size);
  if (p == NULL) {
    return NOD_RESULT_ERR_OTHER;
  }
  p->boot = read_blob(img, 0, 0x440);
  p->bi2 = read_blob(img, 0x440, 0x2000);
  if (p->boot == NULL || p->bi2 == NULL) {
    goto fail;
  }
  // Apploader: 0x20-byte header, then code and trailer sizes.
  uint8_t ah[0x20];
  if (disc_read(img, 0x2440, ah, sizeof ah) != 0) {
    goto fail;
  }
  p->apploaderSize = 0x20 + (size_t)be32(ah + 0x14) + be32(ah + 0x18);
  p->apploader = read_blob(img, 0x2440, p->apploaderSize);
  // main.dol: its size is the end of its furthest section.
  const uint32_t dolOff = be32(p->boot + 0x420);
  uint8_t dh[0x100];
  if (p->apploader == NULL || disc_read(img, dolOff, dh, sizeof dh) != 0) {
    goto fail;
  }
  uint32_t dolSize = 0x100;
  for (int i = 0; i < 18; i++) {
    const uint32_t o = be32(dh + i * 4);
    const uint32_t s = be32(dh + 0x90 + i * 4);
    if (o != 0 && s != 0 && o + s > dolSize) {
      dolSize = o + s;
    }
  }
  p->dolSize = dolSize;
  p->dol = read_blob(img, dolOff, dolSize);
  // fst.bin
  const uint32_t fstOff = be32(p->boot + 0x424);
  const uint32_t fstSize = be32(p->boot + 0x428);
  if (p->dol == NULL || fstSize < 12 || fstSize > (64u << 20)) {
    goto fail;
  }
  p->fstSize = fstSize;
  p->fst = read_blob(img, fstOff, fstSize);
  if (p->fst == NULL) {
    goto fail;
  }
  const uint32_t count = be32(p->fst + 8);
  if (count == 0 || (uint64_t)count * 12 > fstSize) {
    goto fail;
  }
  p->nodeCount = count;
  p->nodes = malloc(sizeof(NlFstNode) * count);
  if (p->nodes == NULL) {
    goto fail;
  }
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* e = p->fst + 12ull * i;
    p->nodes[i].isDir = e[0] != 0;
    p->nodes[i].nameOff = be32(e) & 0xFFFFFF;
    p->nodes[i].offset = be32(e + 4);
    p->nodes[i].length = be32(e + 8);
  }
  p->strings = (const char*)p->fst + 12ull * count;
  p->stringsSize = fstSize - 12ull * count;
  *out = p;
  return NOD_RESULT_OK;
fail:
  set_error("GameCube partition headers are unreadable");
  nod_free(p);
  return NOD_RESULT_ERR_FORMAT;
}

NodResult nod_disc_open_partition(NodHandle* disc, uint32_t index, const NodPartitionOptions* options,
                                  NodHandle** out) {
  if (index != 0) {
    return NOD_RESULT_ERR_NOT_FOUND;
  }
  return nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, options, out);
}

NodResult nod_partition_open_file(NodHandle* partition, uint32_t fst_index, NodHandle** out) {
  if (partition == NULL || partition->kind != NL_PARTITION || out == NULL) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  if (fst_index == 0 || fst_index >= partition->nodeCount || partition->nodes[fst_index].isDir) {
    return NOD_RESULT_ERR_NOT_FOUND;
  }
  const NlFstNode* n = &partition->nodes[fst_index];
  if ((uint64_t)n->offset + n->length > partition->img->size) {
    return NOD_RESULT_ERR_FORMAT;
  }
  NodHandle* h = handle_new(NL_FILE, partition->img, n->offset, n->length);
  if (h == NULL) {
    return NOD_RESULT_ERR_OTHER;
  }
  *out = h;
  return NOD_RESULT_OK;
}

void nod_free(NodHandle* h) {
  if (h == NULL) {
    return;
  }
  free(h->boot);
  free(h->bi2);
  free(h->apploader);
  free(h->dol);
  free(h->fst);
  free(h->nodes);
  free(h->buf);
  image_release(h->img);
  free(h);
}

int64_t nod_read(NodHandle* h, uint8_t* buf, size_t len) {
  if (h == NULL) {
    return -1;
  }
  if (h->pos >= h->len) {
    return 0;
  }
  if (len > h->len - h->pos) {
    len = (size_t)(h->len - h->pos);
  }
  if (disc_read(h->img, h->base + h->pos, buf, len) != 0) {
    set_error("disc read failed");
    return -1;
  }
  h->pos += len;
  return (int64_t)len;
}

int64_t nod_seek(NodHandle* h, int64_t offset, int32_t whence) {
  if (h == NULL) {
    return -1;
  }
  int64_t base = 0;
  if (whence == 1) {
    base = (int64_t)h->pos;
  } else if (whence == 2) {
    base = (int64_t)h->len;
  } else if (whence != 0) {
    return -1;
  }
  const int64_t np = base + offset;
  if (np < 0) {
    return -1;
  }
  h->pos = (uint64_t)np;
  return np;
}

#define NL_BUF_SIZE (1u << 20)

const void* nod_buf_read(NodHandle* h, size_t* out_len) {
  if (out_len) {
    *out_len = 0;
  }
  if (h == NULL || h->pos >= h->len) {
    return NULL;
  }
  if (h->buf == NULL) {
    h->buf = malloc(NL_BUF_SIZE);
    if (h->buf == NULL) {
      return NULL;
    }
  }
  // The buffer holds [bufStart, bufStart + bufLen); refill when pos left it.
  if (h->pos < h->bufStart || h->pos >= h->bufStart + h->bufLen) {
    size_t n = NL_BUF_SIZE;
    if (n > h->len - h->pos) {
      n = (size_t)(h->len - h->pos);
    }
    if (disc_read(h->img, h->base + h->pos, h->buf, n) != 0) {
      set_error("disc read failed");
      h->bufLen = 0;
      return NULL;
    }
    h->bufStart = h->pos;
    h->bufLen = n;
  }
  const size_t off = (size_t)(h->pos - h->bufStart);
  if (out_len) {
    *out_len = h->bufLen - off;
  }
  return h->buf + off;
}

void nod_buf_consume(NodHandle* h, size_t n) {
  if (h != NULL) {
    h->pos += n;
  }
}

NodResult nod_disc_header(const NodHandle* disc, NodDiscHeader* out) {
  if (disc == NULL || out == NULL) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  _Static_assert(sizeof(NodDiscHeader) == 0x400, "NodDiscHeader layout");
  return disc_read(disc->img, 0, out, sizeof *out) == 0 ? NOD_RESULT_OK : NOD_RESULT_ERR_IO;
}

NodResult nod_disc_meta(const NodHandle* disc, NodDiscMeta* out) {
  if (disc == NULL || out == NULL) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  memset(out, 0, sizeof *out);
  out->format = disc->img->format;
  out->compression.kind =
      disc->img->format == NOD_FORMAT_GCZ ? NOD_COMPRESSION_KIND_DEFLATE : NOD_COMPRESSION_KIND_NONE;
  out->block_size = disc->img->blockSize;
  out->lossless = true;
  out->disc_size = disc->img->size;
  return NOD_RESULT_OK;
}

uint64_t nod_disc_size(const NodHandle* disc) {
  return disc ? disc->img->size : 0;
}

size_t nod_disc_partitions(const NodHandle* disc, NodPartitionInfo* out, size_t cap) {
  if (disc == NULL) {
    return 0;
  }
  if (out != NULL && cap > 0) {
    out[0].index = 0;
    out[0].kind = NOD_PARTITION_KIND_DATA;
    out[0].data_size = disc->img->size;
  }
  return 1;
}

bool nod_partition_is_wii(const NodHandle* partition) {
  (void)partition;
  return false;
}

NodResult nod_partition_meta(const NodHandle* p, NodPartitionMeta* out) {
  if (p == NULL || p->kind != NL_PARTITION || out == NULL) {
    return NOD_RESULT_ERR_INVALID_HANDLE;
  }
  memset(out, 0, sizeof *out);
  out->raw_boot = (NodBlob){p->boot, 0x440};
  out->raw_bi2 = (NodBlob){p->bi2, 0x2000};
  out->raw_apploader = (NodBlob){p->apploader, p->apploaderSize};
  out->raw_dol = (NodBlob){p->dol, p->dolSize};
  out->raw_fst = (NodBlob){p->fst, p->fstSize};
  return NOD_RESULT_OK;
}

static const char* node_name(const NodHandle* p, uint32_t i) {
  const uint32_t off = p->nodes[i].nameOff;
  return off < p->stringsSize ? p->strings + off : "";
}

static int name_eq(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char ca = a[i], cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb || ca == 0) {
      return 0;
    }
  }
  return a[n] == 0;
}

uint32_t nod_partition_find_file(const NodHandle* p, const char* path, NodNodeKind* out_kind,
                                 uint32_t* out_length) {
  if (p == NULL || p->kind != NL_PARTITION || path == NULL) {
    return NOD_FST_STOP;
  }
  uint32_t dir = 0;
  const char* s = path;
  while (*s == '/') s++;
  if (*s == 0) {
    if (out_kind) *out_kind = NOD_NODE_KIND_DIRECTORY;
    if (out_length) *out_length = 0;
    return 0;
  }
  for (;;) {
    const char* slash = strchr(s, '/');
    const size_t n = slash ? (size_t)(slash - s) : strlen(s);
    const uint32_t end = dir == 0 ? p->nodeCount : p->nodes[dir].length;
    uint32_t found = NOD_FST_STOP;
    for (uint32_t i = dir + 1; i < end && i < p->nodeCount;) {
      if (name_eq(node_name(p, i), s, n)) {
        found = i;
        break;
      }
      i = p->nodes[i].isDir ? p->nodes[i].length : i + 1;
    }
    if (found == NOD_FST_STOP) {
      return NOD_FST_STOP;
    }
    s += n;
    while (*s == '/') s++;
    if (*s == 0) {
      if (out_kind) *out_kind = p->nodes[found].isDir ? NOD_NODE_KIND_DIRECTORY : NOD_NODE_KIND_FILE;
      if (out_length) *out_length = p->nodes[found].isDir ? 0 : p->nodes[found].length;
      return found;
    }
    if (!p->nodes[found].isDir) {
      return NOD_FST_STOP;
    }
    dir = found;
  }
}

void nod_partition_iterate_fst(const NodHandle* p, NodFstCallback callback, void* user_data) {
  if (p == NULL || p->kind != NL_PARTITION || callback == NULL) {
    return;
  }
  uint32_t i = 1;
  while (i < p->nodeCount) {
    const NlFstNode* n = &p->nodes[i];
    const uint32_t next = callback(i, n->isDir ? NOD_NODE_KIND_DIRECTORY : NOD_NODE_KIND_FILE, node_name(p, i),
                                   n->length, user_data);
    if (next == NOD_FST_STOP || next <= i) {
      break;
    }
    i = next;
  }
}
