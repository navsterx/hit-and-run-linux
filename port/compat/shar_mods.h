/* Mods in the format of Lucas' Simpsons Hit & Run Mod Launcher (port/compat/shar_mods.cpp, shar_modlua.cpp).
 * SHAR_MODS lists mods, ':'-separated, each an extracted mod folder or a .lmlm package (read in place);
 * earlier mods win when two provide the same file (port/play.sh --mod sets it).
 * A main mod (Meta.ini Main=1) gets its own saved games in ModSaves/<InternalName>/ in the game dir.
 * Supported: CustomFiles (a mod's CustomFiles/ tree overrides game files, also those inside RCF archives, also
 * by the sound system's key strings; CustomFiles.ini redirections, occluded paths and Lua path handlers;
 * CustomFiles.lua), OggVorbisSupport (x.ogg serves x.rsd), CustomLimits (only the limits wired up through
 * shar_ModLimit); read by the game through shar_ModConfig: CustomShopSupport (shar_modshops.h), CustomCarSupport (shar_modcars.h),
 * CustomDialogueCharacterCodes ([CharacterCodes] only), CustomText (guitextbible.cpp; English or the first
 * language), 3DPhoneBoothPreviewSupport (guiscreenphonebooth.cpp; not DarkenLockedCars/FullScreen).
 * Other hacks a mod's Meta.ini requires are reported at startup and ignored. Mod settings are
 * their Meta.ini defaults unless ModSettings.ini in the game dir sets them ([InternalName] Name=value).
 * SHAR_MOD_LOG=1 prints every file a mod provides as it is opened.
 * Loaded on first use; malloc only (called from the file system, see the compat-layer rule in CLAUDE.md). */
#ifndef SHAR_COMPAT_SHAR_MODS_H
#define SHAR_COMPAT_SHAR_MODS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A file a mod provides: fd >= 0 is an open file (generated content) the receiver now owns, size bytes from 0;
 * otherwise bytes [offset, offset + size) of the file at path (the whole file for a mod folder's file, a slice
 * of the package for a .lmlm). */
typedef struct SharModFile
{
    const char* path;
    unsigned long long offset;
    unsigned int size;
    int packaged;
    int fd;
} SharModFile;

enum
{
    SHAR_MOD_MISSING = -1,      /* the file does not exist (occluded) */
    SHAR_MOD_NONE = 0,          /* no mod is involved: open the game's own file */
    SHAR_MOD_FILE = 1,          /* *file */
    SHAR_MOD_REDIRECT = 2       /* open the game's own file *redirect instead (it may be inside an RCF archive) */
};

/* What a read-only open of gamePath ("art\l1z1.p3d", any case, optional drive letter, or a sound key string)
 * gets. Runs Lua path handlers, so call it once per open. *redirect stays valid until the next call on this
 * thread. */
int shar_ModResolve(const char* gamePath, SharModFile* file, const char** redirect);

/* The radcore drive's half of an open whose shar_ModResolve the file request (requests.cpp) already made on this
 * thread: SHAR_MOD_FILE and *file when that was for gamePath, else resolves now. */
int shar_ModTakeResolved(const char* gamePath, SharModFile* file, const char** redirect);

/* The game-relative folder holding the main mod's saved games ("ModSaves/<InternalName>"), or NULL. */
const char* shar_ModSaveDir(void);

/* A CustomLimits.ini value ([section] key=value, case-insensitive). Several mods setting it: the largest wins.
 * Returns defaultValue when no mod sets it. */
int shar_ModLimit(const char* section, const char* key, int defaultValue);

/* A config file in a mod's root folder ("CustomShopSupport.xml"), from the first mod that has one, as malloc'd
 * NUL-terminated text the caller frees; NULL when no mod has it. */
char* shar_ModConfig(const char* name);

/* The files directly in a mod's root subfolder ("CustomText"), from the first mod that has any: up to max texts
 * (malloc'd, NUL-terminated after sizes[i] bytes, which may include zeros, e.g. UTF-16) and file names (malloc'd),
 * all for the caller to free. Returns the count. */
int shar_ModConfigFolder(const char* folder, char** texts, size_t* sizes, char** names, int max);

#ifdef __cplusplus
}
#endif

#endif
