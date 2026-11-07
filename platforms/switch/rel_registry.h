#ifndef MP4_SWITCH_REL_REGISTRY_H
#define MP4_SWITCH_REL_REGISTRY_H

// Every REL (minigame, board, menu) linked into the .nro. tools/switch/rel_isolate.cmake renames a
// REL's ObjectSetup to mp4rel_<name>_ObjectSetup, hides every other symbol it defines, and moves
// its writable data into the section mp4ovl_<name>, which the linker brackets with
// __start_/__stop_ symbols. The generated rel_registry.c lists them.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mp4Rel {
    const char *name;           // REL directory name, e.g. "m401Dll"
    void (*objectSetup)(void);
    unsigned char *dataStart;   // initialised writable data, NULL when the REL has none
    unsigned char *dataEnd;
    unsigned char *bssStart;    // zero-initialised data, NULL when the REL has none
    unsigned char *bssEnd;
} Mp4Rel;

extern const Mp4Rel mp4RelTable[];
extern const unsigned mp4RelCount;

// Finds the REL named by an overlay file name ("m401Dll.so"; case-insensitive, extension
// ignored), restores its globals to their initial values and returns it, or NULL.
const Mp4Rel *mp4RelLink(const char *fileName);

#ifdef __cplusplus
}
#endif

#endif
