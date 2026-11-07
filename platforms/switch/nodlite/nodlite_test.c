// Host test for nodlite: nodlite_test <image> lists the FST and checks every file reads.
#include "nod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <xxhash.h>

typedef struct {
  NodHandle* part;
  unsigned files, dirs;
  unsigned long long bytes;
  int failed;
} Ctx;

static uint32_t cb(uint32_t index, NodNodeKind kind, const char* name, uint32_t size, void* ud) {
  Ctx* c = ud;
  if (kind == NOD_NODE_KIND_DIRECTORY) {
    c->dirs++;
    return index + 1;
  }
  c->files++;
  NodHandle* f;
  if (nod_partition_open_file(c->part, index, &f) != NOD_RESULT_OK) {
    printf("open failed: %s\n", name);
    c->failed = 1;
    return index + 1;
  }
  static uint8_t buf[1 << 20];
  uint64_t total = 0;
  for (;;) {
    int64_t n = nod_read(f, buf, sizeof buf);
    if (n < 0) {
      printf("read failed: %s\n", name);
      c->failed = 1;
      break;
    }
    if (n == 0) break;
    total += (uint64_t)n;
  }
  if (total != size) {
    printf("size mismatch: %s %llu != %u\n", name, (unsigned long long)total, size);
    c->failed = 1;
  }
  c->bytes += total;
  nod_free(f);
  return index + 1;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  NodHandle* disc;
  if (nod_disc_open(argv[1], NULL, &disc) != NOD_RESULT_OK) {
    printf("open: %s\n", nod_error_message());
    return 1;
  }
  NodDiscHeader h;
  nod_disc_header(disc, &h);
  printf("id %.6s rev %u title %.64s size %llu\n", h.game_id, h.disc_version, h.game_title,
         (unsigned long long)nod_disc_size(disc));
  Ctx c = {0};
  if (nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, NULL, &c.part) != NOD_RESULT_OK) {
    printf("partition: %s\n", nod_error_message());
    return 1;
  }
  NodPartitionMeta m;
  nod_partition_meta(c.part, &m);
  printf("dol %zu bytes, fst %zu bytes, apploader %zu bytes\n", m.raw_dol.size, m.raw_fst.size,
         m.raw_apploader.size);
  NodNodeKind k;
  uint32_t len;
  uint32_t idx = nod_partition_find_file(c.part, "/data/E3setup.bin", &k, &len);
  printf("find /data/E3setup.bin -> %u (%u bytes)\n", idx, len);
  if (argc > 2) {
    unsigned long crc = crc32(0, NULL, 0);
    XXH3_state_t* xs = XXH3_createState();
    XXH3_128bits_reset(xs);
    size_t n;
    const void* b;
    while ((b = nod_buf_read(disc, &n)) != NULL && n) {
      crc = crc32(crc, b, (uInt)n);
      XXH3_128bits_update(xs, b, n);
      nod_buf_consume(disc, n);
    }
    XXH128_hash_t hh = XXH3_128bits_digest(xs);
    printf("disc crc32 %08lx xxh128 %016llx%016llx\n", crc, (unsigned long long)hh.high64,
           (unsigned long long)hh.low64);
  }
  nod_partition_iterate_fst(c.part, cb, &c);
  printf("%u files, %u dirs, %llu bytes, %s\n", c.files, c.dirs, c.bytes, c.failed ? "FAILED" : "ok");
  nod_free(c.part);
  nod_free(disc);
  return c.failed;
}
