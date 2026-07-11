/* Mods in the format of Lucas' Simpsons Hit & Run Mod Launcher (port/compat/shar_mods.cpp).
 * SHAR_MODS lists mods, ':'-separated, each an extracted mod folder or a .lmlm package (read in place);
 * earlier mods win when two override the same file (port/play.sh --mod sets it).
 * Supported so far: CustomFiles without Lua (a mod's CustomFiles/ tree overrides game files, also those inside
 * RCF archives) and CustomLimits (only the limits wired up through shar_ModLimit). Other hacks a mod's Meta.ini
 * requires are reported at startup and ignored. SHAR_MOD_LOG=1 prints every overridden file as it is opened.
 * Loaded on first use; malloc only (called from the file system, see the compat-layer rule in CLAUDE.md). */
#ifndef SHAR_COMPAT_SHAR_MODS_H
#define SHAR_COMPAT_SHAR_MODS_H

#ifdef __cplusplus
extern "C" {
#endif

/* A file a mod provides: bytes [offset, offset + size) of the file at path (offset 0 and the whole file for a
 * mod folder; a slice of the package for a .lmlm). */
typedef struct SharModFile
{
    const char* path;
    unsigned long long offset;
    unsigned int size;
    int packaged;
} SharModFile;

/* Finds the mod file overriding gamePath ("art\l1z1.p3d", any case, optional drive letter). Returns 0 if none. */
int shar_ModFindFile(const char* gamePath, SharModFile* file);

/* Whether a mod overrides gamePath (shar_ModFindFile without the SHAR_MOD_LOG line). */
int shar_ModHasFile(const char* gamePath);

/* A CustomLimits.ini value ([section] key=value, case-insensitive). Several mods setting it: the largest wins.
 * Returns defaultValue when no mod sets it. */
int shar_ModLimit(const char* section, const char* key, int defaultValue);

#ifdef __cplusplus
}
#endif

#endif
