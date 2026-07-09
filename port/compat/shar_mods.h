/* Mods in the format of Lucas' Simpsons Hit & Run Mod Launcher (port/compat/shar_mods.cpp).
 * SHAR_MODS lists extracted mod folders, ':'-separated; earlier mods win when two override the same file
 * (port/play.sh --mod sets it, extracting .lmlm packages with mod/tools/lmlm_extract.py).
 * Supported so far: CustomFiles without Lua (a mod's CustomFiles/ tree overrides game files, also those inside
 * RCF archives) and CustomLimits (only the limits wired up through shar_ModLimit). Other hacks a mod's Meta.ini
 * requires are reported at startup and ignored. SHAR_MOD_LOG=1 prints every overridden file as it is opened.
 * Loaded on first use; malloc only (called from the file system, see the compat-layer rule in CLAUDE.md). */
#ifndef SHAR_COMPAT_SHAR_MODS_H
#define SHAR_COMPAT_SHAR_MODS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The real path of a mod file overriding gamePath ("art\l1z1.p3d", any case, optional drive letter), or NULL. */
const char* shar_ModFindFile(const char* gamePath);

/* Whether a mod overrides gamePath (shar_ModFindFile without the SHAR_MOD_LOG line). */
int shar_ModHasFile(const char* gamePath);

/* A CustomLimits.ini value ([section] key=value, case-insensitive). Several mods setting it: the largest wins.
 * Returns defaultValue when no mod sets it. */
int shar_ModLimit(const char* section, const char* key, int defaultValue);

#ifdef __cplusplus
}
#endif

#endif
