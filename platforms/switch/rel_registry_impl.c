// mp4RelLink: the static stand-in for LoadLibrary/dlopen of a REL.
#include "rel_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// The first link of a REL copies its pristine initialised data aside; each later link copies it
// back and clears its .bss, so a REL starts from its initial state every time, as it would from a
// freshly loaded DLL.
static unsigned char **s_snapshots;
static unsigned char *s_linked;

const Mp4Rel *mp4RelLink(const char *fileName)
{
    if (fileName == NULL) {
        return NULL;
    }
    const char *base = strrchr(fileName, '/');
    base = base ? base + 1 : fileName;
    size_t len = strlen(base);
    const char *dot = strrchr(base, '.');
    if (dot != NULL) {
        len = (size_t)(dot - base);
    }
    if (s_snapshots == NULL) {
        s_snapshots = calloc(mp4RelCount, sizeof *s_snapshots);
        s_linked = calloc(mp4RelCount, 1);
        if (s_snapshots == NULL || s_linked == NULL) {
            return NULL;
        }
    }
    for (unsigned i = 0; i < mp4RelCount; i++) {
        const Mp4Rel *rel = &mp4RelTable[i];
        if (strlen(rel->name) != len || strncasecmp(rel->name, base, len) != 0) {
            continue;
        }
        if (!s_linked[i]) {
            if (rel->dataStart != NULL && rel->dataEnd > rel->dataStart) {
                const size_t size = (size_t)(rel->dataEnd - rel->dataStart);
                s_snapshots[i] = malloc(size);
                if (s_snapshots[i] != NULL) {
                    memcpy(s_snapshots[i], rel->dataStart, size);
                }
            }
            s_linked[i] = 1;
        } else if (s_snapshots[i] != NULL) {
            memcpy(rel->dataStart, s_snapshots[i], (size_t)(rel->dataEnd - rel->dataStart));
        }
        // Cleared on every link, the first included: the sections sit past crt0's own .bss range.
        if (rel->bssStart != NULL && rel->bssEnd > rel->bssStart) {
            memset(rel->bssStart, 0, (size_t)(rel->bssEnd - rel->bssStart));
        }
        return rel;
    }
    fprintf(stderr, "[rel] no REL named %.*s in this build\n", (int)len, base);
    return NULL;
}
