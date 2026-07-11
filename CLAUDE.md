# CLAUDE.md: The Simpsons Hit & Run, native Linux port

Port of the original 2003 source to 32-bit Linux with SDL2 + OpenGL. The untouched original dump is at
`~/Documents/Decomp/hitnrun_source/game` (never modify it). This repo holds only the files the build uses
(sources, included headers, the VS project files `gen_cmake.py` reads, and the Win32 originals it swaps for Linux ones).
Status (2026-10-14): builds, boots, plays the front end and gameplay (tested into Level 7) with sound, music,
dialogue and movies, at any window aspect ratio. Works with the dump's PC data and the retail PC release.
Missing: EAX reverb, force feedback. `README.md` has the user-facing status checklist; `RUST.md` holds notes for a
possible future Rust/Bevy reimplementation (not started).

The source is leaked/proprietary: keep everything private and use game data from your own copy.

## Layout

```
code/, libs/, build/       copy of game/code, game/libs, game/build (sources + VS projects only), git-tracked
port/                      everything added for the port
  vcx.py                   reads the original VS2013 .vcxproj files (case-insensitive paths)
  gen_cmake.py             generates CMakeLists.txt from them (never edit CMakeLists.txt by hand)
  fix_include_case.py      rewrites #include paths to on-disk case (already applied)
  gen_dinput.py            generates compat/dinput.h from the bundled DX8 header
  syntax_survey.py         g++ -fsyntax-only over every source, groups first errors
  check.py FILE...         syntax-check files with their target's exact flags
  run.sh [secs] [args]     rebuild + run under gdb from the run dir, print backtrace on crash
  play.sh --data DIR       build if needed, set up the run dir from DIR (game data), start the game normally
                           (--debug, --no-build, --mod .lmlm|FOLDER (repeatable); extra args go to the game)
  memcheck.sh [secs] [args]  run under valgrind memcheck (fetched/set up on first use, no root) and summarise
                           uninitialised reads by origin (memcheck_summary.py); see lessons
  compat/                  force-included msvc_compat.h, types-only windows.h, crtdbg.h, dinput.h,
                           win32_threads.* (threads/sync/fibers/timers on POSIX), win32_misc.* (MessageBox),
                           pddi_heap.cpp (renderer's private heap), valgrind/ (client-request headers),
                           shar_mods.* (Mod Launcher mods, see below)
mod/tools/                 modding tools: lmlm_extract.py (unpacks Mod Launcher .lmlm "LSPA" packages)
build-linux/               release build (RAD_RELEASE)     cmake -B build-linux -G Ninja
build-linux-debug/         debug build (RAD_DEBUG: asserts + Radical's debug output)   -DSHAR_RAD_DEBUG=ON
build-linux-audit/         -DSHAR_UNINIT_AUDIT=ON -DCMAKE_BUILD_TYPE=Release: all warnings on; grep -Wuninitialized /
                           -Wmaybe-uninitialized (2 left 2026-10-13, both false positives: actordsg.cpp, opticlensflare.cpp)
run/                      run dir (git-ignored): symlinks to game data, private simpsons.ini/Save1-4 (play.sh sets it
                           up; currently the user's retail copy). For experiments use a separate run dir in the
                           scratchpad (SHAR_RUN=...) with its own ini, e.g. display=window, resolution=1280x720
```

Remote: `origin` = github.com/zeropaulsy/simpsons-hit-and-run-recomp (private; must stay private). Commit as
`zeropaulsy <13328895+zeropaulsy@users.noreply.github.com>`. Pushing needs the user's credentials (they push themselves).

## Commands

```bash
python3 port/gen_cmake.py                 # after changing port rules; then ninja regenerates
ninja -C build-linux-debug -j6
SHAR_BUILD=$PWD/build-linux-debug port/run.sh 120      # absolute SHAR_BUILD path
SHAR_SHOT_EVERY=10 ...                    # renderer saves shot_NNN.png every 10 s in the run dir
SHAR_ASSERT_ABORT=1 ...                   # make failed asserts abort instead of log-and-ignore
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 MESA_DEBUG=1 ...   # Mesa reports GL errors
python3 port/syntax_survey.py; python3 port/check.py code/foo.cpp
srr2 skipfe                               # straight into Level 1; `skipfe l7 m3` = level 7 mission 3 (index from
                                          # MISSION_TITLE_L<l>_M<m> in art/frontend/scrooby2/resource/txtbible/srr2.txt;
                                          # races can't be started this way). l7 m3 (Pocket Protector) starts outdoors.
SHAR_KEY_SCRIPT="17@30000-60000,..."      # scripted input; the retail ini binds Action to E (18), menus take Enter (28)
SHAR_FILL_ALLOC=0xFF / SHAR_FILL_PDDI=0xFF  # fill new game / renderer allocations to expose uninitialised reads
SHAR_RUN=<scratch run dir> port/memcheck.sh 330 skipfe   # memcheck into Level 1 (~4 min to playable), summary at end
```

## Platform model

- Linux is a PC platform: `RAD_WIN32` stays defined (all PC game paths), `RAD_LINUX` marks OS-API differences.
  Also `LINUX` (old Pure3D headers), `RAD_PC`, `PDDI_NO_ASM`. In Pure3D, Win32 blocks yield to `RAD_LINUX`.
- Generator rules (`gen_cmake.py`): `win32/` sources replaced by a `linux/` sibling (`win32drive.cpp` ->
  `linux/linuxdrive.cpp`) or excluded; `LINUX_EXCLUDE` / `LINUX_EXTRA` per target; DX8 pddi replaced by GL pddi;
  bundled Windows SDK include dirs dropped.
- Ported subsystems: Pure3D platform + GL display on SDL2; radcore drive (POSIX, case-insensitive paths, every
  drive letter = game dir), controllers (SDL2, DirectInput-compatible points), platform, stack trace; game
  window/event loop (`code/main/linuxplatform_sdl.cpp`, `Win32Platform::PumpEvents`); radsound HAL
  (`hal/linux/linuxhal.cpp`, SDL audio-callback mixer + software 3D listener); movies: the original
  `binkmovieplayer.cpp` on a Bink-SDK stand-in (`radmovie/src/linux/bink.h`, `binkffmpeg.cpp`: FFmpeg decode, audio
  into the mixer's movie stream). Needs `libavformat-dev:i386 libavcodec-dev:i386 libswscale-dev:i386
  libswresample-dev:i386 libavutil-dev:i386`;
  radscript `InvokeVf` in C++; debug-comm link stubbed.

## Hard-won lessons (Windows -> Linux differences that bit)

- **Allocator isolation**: the game replaces global `operator new`. On ELF that leaks into every shared library.
  Fixed by `-static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL`, `SHAR_LOCAL` (hidden) on the game's
  operators, and objcopy-renaming pddigl's new/delete to `pddi_heap_*` (as pddidx8.dll had its own heap).
- **Compat layer must never use C++ new** (it runs inside the game's lazy heap initialisation): malloc only.
- **CreateThread stack size is a commit size**: Windows still reserves 1 MB; pthreads get >= 1 MB.
- **D3D8 is multithreaded, GL is not**: Pure3D loads/sets state on worker threads. Workers get shared GL
  contexts (`shar_SetThreadStartHook` / `shar_RunThreadStartHook`, called in radcontent's load loop); texture
  uploads are lazy on the render thread; max texture size cached.
- `pglDisplay::SwapBuffers` must clear `reset` (else every texture re-uploads each frame and leaks).
- PC data has DXT textures: decoded to RGBA in software (`p3d/dxtn.cpp`) since the GL pddi has none.
- radscript finds script classes via `typeid().name()`: MSVC format assumed; Linux demangles (`radtypeinfo.hpp`).
  Vtable slots in `srrtypes.typ` are MSVC's; they match GCC only without virtual dtors/overloads in scripted
  interfaces (true so far).
- **Uninitialised reads were zero by luck under MSVC**: e.g. `BipedCharacterMappable::GetDirection` never set
  `direction.y`, so in mouse-look mode Homer ran on his own. Fixed sites are marked `// uninit fix (Linux port)`;
  rerun the audit build and `port/memcheck.sh` after porting new code. Fix by initialising to zero (what the
  shipped game effectively saw), not to what looks intended. Under valgrind every game heap / FBMemoryPool
  allocation is marked undefined, so recycled memory counts too; valgrind must not intercept the game's operator
  new (`--soname-synonyms=somalloc=nouserintercepts`) and needs an unstripped i386 ld.so (the script builds one).
  To find which member is undefined, `VALGRIND_GET_VBITS` on the object plus `gdb -batch -ex "ptype /o Class"`
  maps byte offsets to members. Origins follow copies: the bad member may be far from the reported use. Input debug env vars: `SHAR_INPUT_LOG`, `SHAR_NO_INPUT`,
  `SHAR_NO_MOUSE`, `SHAR_KEY_SCRIPT="dik@startms-endms,..."` (1000+n = mouse button n), `SHAR_FORCE_MOUSELOOK`.
- **Texture row order**: both pddis' `Lock()` hand out a bottom-up view (negative pitch), except D3D8 skips that
  for DXT formats and copies blocks as-is. The software DXT decoder (`dxtn.cpp`) therefore delivers rows with
  `Builder::BOTTOM` to cancel the GL flip; with `TOP`, DXT images (sprites, HUD portraits) came out upside down.
- **Frame timing**: D3D8 presented at the display rate; unsynced GL ran at 2,000-4,000 fps with mostly 0 ms frames
  (ms timer), which the 30-60 fps physics/AI never handled: traffic speed 0/0 = NaN (-> NaN engine sounds), cars
  launching over slopes. Now vsync on (`gldisplay.cpp`) plus a 60 fps cap in the main loop (`game.cpp`,
  `SHAR_FPS_CAP=n`, 0 = off). `SHAR_FRAME_LOG=1` prints a frame-time histogram every 5 s.
  Side effect found: radcontent's load thread only runs while the main thread is inside `SwitchTasks()` (one
  mutex hand-off per frame, and it hands back after every file read), so at 60 fps boot stalled on the legal
  screen. `radLoadManager::Service` now keeps handing off for up to 8 ms per frame (`SHAR_LOAD_BUDGET_MS`).
  `SHAR_VSYNC=0` turns vsync off for testing.
- **GCC 15 libstdc++ asserts in unoptimised builds**: `vector::operator[]` past the end aborts (EventManager read
  `listeners[i]` after removal). Disabled outside the debug build (`-D_GLIBCXX_NO_ASSERTIONS`), like MSVC release.
- **Line endings**: most original sources are CRLF. Edit with tools that keep them (Python: `open(f, newline='')`
  or bytes; `Path.read_text()` silently converts to LF and rewrites the whole file).
- **Texture clamp**: D3D's CLAMP is GL_CLAMP_TO_EDGE; GL_CLAMP (what the GL pddi had) blends the black border in
  under bilinear filtering: dark seams between movie tiles, dark texture edges (`glmat.cpp` uvTable).
- **Any aspect ratio / resizable window** (`p3d/displayaspect.hpp`): cameras set to 4:3 render Hor+ (wider, same
  vertical view; Vert+ on narrower windows) through effective values inside `tCamera` (`GetFOV` still returns the
  game's 4:3 values: cutscene animations and camera blends read them back). Culling/world-to-screen use
  `GetEffectiveFOV`. The 640x480 Scrooby UI is drawn in the centred 4:3 area and clipped to it (letterbox bars wait
  off-page); polygons spanning the page width/height (fades, letterbox) stretch to the window edge; a full-page
  `FePure3dObject` (main-menu room) fills the window, smaller ones stay in their 4:3 box; the front end's TV frame
  sprite opts in to stretching over the window (`Scrooby::Sprite::SetStretchToWindow`). The GL display follows the
  window size each swap. Test resizing with `SHAR_RESIZE_SCRIPT="WxH@ms,..."`.
- **Resolutions / fullscreen**: `Win32Platform::Resolution` values index a runtime table (`linuxplatform_sdl.cpp`):
  the enum's ten entries first (values unchanged), then common PC sizes, the desktop size and any WxH read from
  simpsons.ini. The display menu (`CGuiScreenDisplay::BuildResolutionList`) shows those that fit the desktop,
  sorted, overwriting Display.pag's strings. Display Mode has a third value, Borderless (`display=borderless`,
  `PDDI_DISPLAY_FULLSCREEN_DESKTOP`): fullscreen at the desktop size, resolution ignored. Full Screen
  (`display=fullscreen`) is exclusive: `gldisplay.cpp` sets the closest display mode to the resolution (renders below
  native, e.g. for speed; keep for a future Android port), leaving fullscreen first or the size does not change
  under XWayland. The display's own mode list is not shown (XWayland reports ~35 emulated modes).
- **The GL pddi had never been finished against D3D8 behaviour** (the shipped game used pddidx8). Fixed so far:
  `SetColourWrite` was missing (masks drawn with colour off, e.g. the HUD map's round "hole", showed black);
  `glClear` honours write masks while D3D's Clear does not (`pglContext::Clear` opens them); `alphaRef` was never
  initialised and `PDDI_SP_ALPHACOMPARE_THRESHOLD` not handled, so alpha-tested objects (chain-link fences, grated
  ramps) vanished per session while their collision stayed (default 0.5, clamped, as `dx8/shaders/shader.cpp`).
  When something renders wrong, compare with the D3D8 pddi in the original dump (`libs/pure3d/pddi/dx8`).
- **Retail PC data differs from the dump's**: music split into `music00-03.rcf` (registered on PC, slot count 10 =
  loading manager's max); dialogue is Radical ADPCM ("RADP"), which the game never decoded outside radmusic because
  it passes a PCM format for every stream: `radSoundRsdFileDataSource` now always reads the header and decodes RADP
  to PCM; some copies replace logo movies with tiny Ogg files, so `BinkOpen` rejects non-Bink files (FFmpeg played
  them at 1280x720 and overflowed the 9-tile movie renderer).
- **Render tree (DSG) arrays**: `SwapArray::Add` wrote past the end when full in release builds; it now refuses,
  and `WorldScene` prints `DSG: ...` lines for objects it cannot place/remove/move (an object missing from the tree
  is invisible but keeps its collision). None seen so far.
- **Cursor under XWayland**: a hidden cursor (SDL_ShowCursor off, or a fully transparent one) that has been warped
  makes XWayland lock the pointer in the window until the cursor is shown, grab or not; it lets go only when the
  pointer first leaves, then snaps it back to the last warp position. Only mouse-look hides the cursor; menus use
  a 1-pixel alpha-1/255 cursor (`Win32Platform::ShowTheCursor`) so the pointer can leave (and the window be resized).
  Gameplay only captures the pointer when mouse-look is on (`FEMouse::SetupInGameMode`; only PC_CAM reads the mouse).
- Boot movies only play in release builds (`SHOW_MOVIES` needs `RAD_RELEASE`, `bootupcontext.cpp`): test them with
  `build-linux`, not the debug build.
- radcore's `radoptions.hpp` enables debug comm / watcher / memory monitor in debug builds: off on Linux.

## Mods (Lucas' Mod Launcher format)

Mods are folders: `Meta.ini` (`RequiredHack=` lines), `CustomFiles/` (overrides game paths), per-hack config files,
optionally Lua (`CustomFiles.lua`, `CustomFiles.ini` path handlers). Distributed as `.lmlm`: not a zip but an "LSPA"
package (512-byte sectors, one field per sector; format notes in `mod/tools/lmlm_extract.py`).
`port/compat/shar_mods.cpp` loads the mods in `SHAR_MODS` (':'-separated, earlier wins; `play.sh --mod`), each a
.lmlm read in place or an extracted folder, and reports hacks it ignores. Supported: CustomFiles without Lua (index
of lower-cased game paths; `requests.cpp` skips the RCF search and `linuxdrive.cpp` opens the mod copy, read-only
opens only; a packaged file is an fd on the .lmlm plus a per-fd offset/size slice that reads are confined to) and
CustomLimits `[Billboards]` (`BillboardQuadManager`, default 25 groups x 100 quads). `SHAR_MOD_LOG=1` logs each
override. Tested 2026-10-14/15 with "SHAR Remastered" 1.0 (Level 1 retexture), as .lmlm and as a folder; package
reads match the extractor's output for it and the Futurama demo (866 files). Keep local packages in `mod/lmlm/`
(git-ignored). Not yet: Lua, other hacks, mod save dirs (`Main=1`), packages over 2 GB (32-bit off_t).
Docs: docs.donutteam.com/docs/LucasSimpsonsHitAndRunModLauncher/.

## Known issues / next steps

1. (fixed 2026-10-13) 2D sprites upside down / HUD portraits looking clipped: DXT row order, see lessons.
2. (done 2026-10-13) Audio: SDL mixer in `linuxhal.cpp`. The mixer owns voice play positions; without a device
   (or `SHAR_NO_AUDIO=1`) voices advance on a timer. Not done: EAX reverb (aux sends ignored).
   Test without speakers: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` (float32 stereo, 48 kHz).
3. (done 2026-10-13) Movies: Bink via FFmpeg, see platform model. Clock is wall time; audio decoded ~0.5 s ahead.
4. Force feedback via SDL haptics/rumble (controller output points).
5. (done 2026-10-14) Input verified in play by the user: keyboard, mouse, wired and wireless controllers.
6. (done 2026-10-14) Release build used throughout testing. 64-bit later (a -m64 syntax survey found ~80
   pointer-to-int casts, ~35 int-to-pointer casts; fixed-size game heaps would need enlarging); modern GL later.
7. (done 2026-10-14) Missing fences/ramps in Level 7: alphaRef fix above; HUD and level transparency confirmed
   in play by the user.
8. (done 2026-10-14) Uninitialised-read hunt with memcheck: boot, Level 1, walking outdoors and the first mission
   are clean (Character parent transforms/ground data, InteriorManager gag flag, TrafficAI state, Context state,
   sim joint state and cloned articulated objects, DSG rank/geo flags). Level 1 now starts identically with
   `SHAR_FILL_ALLOC=0x00` and `0xFF`. Not yet covered under memcheck: driving, interiors, later levels, races.
9. (done 2026-10-14) Widescreen verified by the user: scales correctly at all ratios, including extreme ones.
   Borderless fullscreen and the runtime resolution list added the same day.
