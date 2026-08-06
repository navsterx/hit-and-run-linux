# CLAUDE.md: The Simpsons Hit & Run, native Linux port

Port of the original 2003 source to 32-bit Linux with SDL2 + OpenGL. The untouched original dump is at
`~/Documents/Decomp/hitnrun_source/game` (never modify it). This repo holds only the files the build uses
(sources, included headers, the VS project files `gen_cmake.py` reads, and the Win32 originals it swaps for Linux ones).
Status (2026-10-17): builds, boots, plays the front end and gameplay (tested into Level 7) with sound, music,
dialogue, movies, reverb and rumble, at any window aspect ratio. Works with the dump's PC data and the retail PC release.
Mod Launcher mods partly supported (8 hacks; the Futurama demo plays Level 1 with its shops and phone booth).
Missing: steering-wheel force feedback. `README.md` has the user-facing status checklist; `RUST.md` holds notes for a
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
                           console output -> run/logs/last-session.log; on a crash / non-zero exit / Ctrl+C it is
                           kept as run/crashes/crash-<date>-<time>.log, backtrace resolved (crash_symbolize.py)
  crash_symbolize.py BIN [LOG]  resolve a crash report's module+offset frames with addr2line
  memcheck.sh [secs] [args]  run under valgrind memcheck (fetched/set up on first use, no root) and summarise
                           uninitialised reads by origin (memcheck_summary.py); see lessons
  compat/                  force-included msvc_compat.h, types-only windows.h, crtdbg.h, dinput.h,
                           win32_threads.* (threads/sync/fibers/timers on POSIX), win32_misc.* (MessageBox),
                           shar_crash.* (SIGSEGV/BUS/ILL/FPE/ABRT -> crash report on stderr; SHAR_CRASH_HANDLER=0 off),
                           pddi_heap.cpp (renderer's private heap), valgrind/ (client-request headers),
                           shar_mods.* (Mod Launcher mods, see below)
mod/tools/                 modding tools: lmlm_extract.py (unpacks Mod Launcher .lmlm "LSPA" packages)
port/third_party/lua/      Lua 5.4.8 (MIT), for mods' scripts
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
                                          # MISSION_TITLE_L<l>_M<m> in art/frontend/scrooby2/resource/txtbible/srr2.txt).
                                          # l7 m3 (Pocket Protector) starts outdoors.
srr2 skipfe race2                         # 2 s after the HUD appears, jump into the level's race N (bonus missions in
                                          # level.mfk order: 1 time trial, 2 circuit, 3 waypoint, 4 wager); the briefing
                                          # waits for Enter (28), then the player's car is placed at the start line
srr2 skipfe demo l3                       # level 3's attract demo: the race AI drives the family car (demo.mfk/d1i.mfk:
                                          # race route, traffic, opponent, HUD), no time limit; for driving tests
srr2 skipfe incar l7 m3                   # normal gameplay, but 2 s after loading the player is put in the mission's
                                          # car wherever it is parked (not from an interior); for player-driven tests
SHAR_KEY_SCRIPT="17@30000-60000,..."      # scripted input; the retail ini binds Action to E (18), menus take Enter (28)
SHAR_FILL_ALLOC=0xFF / SHAR_FILL_PDDI=0xFF  # fill new game / renderer allocations to expose uninitialised reads
SHAR_RUN=<scratch run dir> port/memcheck.sh 330 skipfe   # memcheck into Level 1 (~3 min to playable), summary at end
SHAR_MAX_FRAME_MS=33 ...                  # cap game time per frame (slow motion): under memcheck the world then streams
                                          # in ahead of the car
```

## Platform model

- Linux is a PC platform: `RAD_WIN32` stays defined (all PC game paths), `RAD_LINUX` marks OS-API differences.
  Also `LINUX` (old Pure3D headers), `RAD_PC`, `PDDI_NO_ASM`. In Pure3D, Win32 blocks yield to `RAD_LINUX`.
- Generator rules (`gen_cmake.py`): `win32/` sources replaced by a `linux/` sibling (`win32drive.cpp` ->
  `linux/linuxdrive.cpp`) or excluded; `LINUX_EXCLUDE` / `LINUX_EXTRA` per target; DX8 pddi replaced by GL pddi;
  bundled Windows SDK include dirs dropped.
- Ported subsystems: Pure3D platform + GL display on SDL2 (OpenGL 3.3 core, see lessons); radcore drive (POSIX, case-insensitive paths, every
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
- PC data has DXT textures (only in sprites: 4,083 front-end/HUD/loading images, 39 MB; models are all PNG): kept
  compressed and uploaded with glCompressedTexImage2D (`gltex.cpp`, 2026-10-17), `p3d/dxtn.cpp` is the original
  again. Without GL_EXT_texture_compression_s3tc (or with `SHAR_GL_NO_S3TC=1`) `gltex.cpp` decodes them in software.
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
- **Texture row order**: both pddis' `Lock()` hand out a bottom-up view (negative pitch), except for DXT formats,
  whose blocks are copied as-is (D3D8's FillLockInfo; `pglTexture::Lock` does the same), so the art stores DXT
  images bottom row first. A decoder feeding scanlines through the bottom-up view must use `Builder::BOTTOM`; with
  `TOP` (the old software path), sprites and HUD portraits came out upside down.
- **Frame timing**: D3D8 presented at the display rate; unsynced GL ran at 2,000-4,000 fps with mostly 0 ms frames
  (ms timer), which the 30-60 fps physics/AI never handled: traffic speed 0/0 = NaN (-> NaN engine sounds), cars
  launching over slopes. Now vsync on (`gldisplay.cpp`) plus a 60 fps cap in the main loop (`game.cpp`,
  `SHAR_FPS_CAP=n`, 0 = off). `SHAR_FRAME_LOG=1` prints a frame-time histogram every 5 s.
  Side effect found: radcontent's load thread only runs while the main thread is inside `SwitchTasks()` (one
  mutex hand-off per frame, and it hands back after every file read), so at 60 fps boot stalled on the legal
  screen. `radLoadManager::Service` now keeps handing off for up to 8 ms per frame (`SHAR_LOAD_BUDGET_MS`).
  `SHAR_VSYNC=0` turns vsync off for testing.
  Hidden window: the NVIDIA driver throttles presenting a minimised window (or one on a sleeping display) to ~1
  swap/s, and loads advance once per frame, so a load left in the background crawled (boot sat on the legal screen).
  `gldisplay.cpp` (`SwapWithVsync`) pauses vsync when swaps are throttled and retries after 5 s, backing off to
  30 s (XWayland does not report minimising reliably). Test: `SHAR_MINIMIZE_SCRIPT="startms-endms,..."`.
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
  Gamma (2026-10-17) was stored and never applied: D3D8 set the card's gamma ramp; `gldisplay.cpp` (`ApplyGamma`)
  now draws the back buffer through a GLSL pass before each swap (all window modes; skipped at gamma 1), as SDL's
  ramp does nothing under (X)Wayland and would recolour the desktop in a window.
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
  The game's own drawn menu cursor hides (and stops hovering) after gamepad input until the mouse moves or clicks
  (`FEMouse::SetHiddenForGamepad`, fed from `PumpEvents`; the revealing click is swallowed).
- **Missing files**: the original retried a failed read forever behind an error screen (the consoles' "put the disc
  back" loop), so a missing file hung the game for good. `Win32Platform::OnDriveError` now prints
  `File not found: <path>` and fails the request; Pure3D's `tFileFTT` loads a file that failed to open as empty
  (it used to read through it and smash the stack). Files a mod occludes set the drive's error too
  (`radDrive::SetLastError`): with the drive's error still Success the handler retried. The game carries on when
  it can do without the file (tested: level01\jumps.p3d); art it needs (art\wrench.p3d) still crashes later.
- Boot movies only play in release builds (`SHOW_MOVIES` needs `RAD_RELEASE`, `bootupcontext.cpp`): test them with
  `build-linux`, not the debug build.
- **A black or frozen window at large sizes may be out of VRAM, not a game bug**: on 2026-10-17 gnome-shell had leaked
  ~7 GB of the 8 GB card after a day of test runs; at 2560x1600 the game showed a stuck legal screen / black frames
  (sound still playing; crashes inside libGL from loader threads), 640x480 worked and older builds "broke" too.
  Logging out and in fixed it. Check `nvidia-smi` before bisecting. The crashes: `InitDisplay`'s result was ignored
  (`p3d/platform/linux/platform.cpp`), so a failed context (BadAlloc) or no worker contexts left loader threads
  making GL calls without a context. Now an error dialog and exit; `GetMaxTextureDimension` never calls GL; a texture
  upload failing with GL_OUT_OF_MEMORY is reported once and retried. Reproduce with a VRAM filler (a GL program
  allocating 4096x4096 textures until ~450 MB are left).
- radcore's `radoptions.hpp` enables debug comm / watcher / memory monitor in debug builds: off on Linux.
- **OpenGL 3.3 core renderer** (2026-10-17, branch dev): the GL pddi was fixed-function GL 1.x. Now
  `pddi/gl/glpipeline.cpp` keeps that state on the CPU (matrices, 8 lights, fog, material, the "current"
  colour/normal/uv) and draws through one GLSL program reproducing what fixed function computed with the pddi's
  settings: per-vertex lighting, separate specular, normals not renormalised (GL_NORMALIZE was never on), GL_MODULATE,
  linear fog on |eye z|, alpha test. Light positions are transformed by the model-view loaded when they are set, as
  glLightfv did. Prim buffers upload to GPU buffers lazily in `Display` (loader threads fill them); glBegin-style
  streams go through a ring buffer. `display_linux/gl.hpp` includes `GL/glcorearb.h`, so a call to a removed GL
  function does not compile; glvnd's libGL exports all entry points (no loader). `pglProgram` is the shader wrapper
  for further shaders; the gamma pass uses it. Every shader name still maps to the "simple" material (D3D8 had 12:
  environment, lightmap, layered, projtex, pointsprite, toon...). Verified against the GL 1.x build by screenshots
  (legal screen identical, flat areas within 1 level; movies, front end, L1, L7 in/outdoors, L3 demo) and equal speed
  (~490 fps uncapped in L1). Uniforms are re-sent only when values change (the game sets the same material ~250x a
  frame; resending all of it cost 30%). `SHAR_GL_DEBUG=1` debug context, driver messages on stderr;
  `SHAR_GL_STATS=1` per-frame draws/uploads and CPU/GPU render time every 5 s. `perf` is not permitted on this machine.
  Also fixed: `~pglTexture` never freed its GL texture (VRAM leaked on every level change).
  Materials (2026-10-17): the retail data uses only three pddi shaders (simple 13,896, spheremap 125 = car paint,
  environment 32; a p3d shader-chunk survey); the game's code asks only for simple/error. `gldev.cpp` maps names as
  the D3D8 pddi did (`pddiBaseShader::InstallShader`; unknown names -> simple); one GLSL program per kind
  (`pglProgramType`, built from shared sources with `#define`s; each program tracks which state versions it has).
  Reflection = D3D8 `d3dReflShader`: diffuse*texture + envBlend*reflection map at the camera-space reflection vector
  (0.5x+0.5, 0.5-0.5y), saturated; the game sets envBlend per frame (`geometryvehicle.cpp`: damage, interiors).
  Lighting/material now follow D3D8, not the old GL pddi: normals renormalised, local viewer, specular only with
  shininess > 0, point-light range cut-off, defaults of `d3dShader` (black diffuse), two-sided materials disable
  culling (`pglContext::SetMaterialTwoSided`; 2,836 materials, ignored before), EMISSIVEALPHA = material alpha
  (traffic/character/vehicle fades), D3D8's blend table with equations (subtract drew opaque, shadow volumes use it).
  Not ported (unused): lightmap, layered, projtex, pointsprite, toon; multi-CBV (2 front-end materials, mode = plain);
  flat shading (25 materials draw Gouraud).

## Mods (Lucas' Mod Launcher format)

Mods are folders: `Meta.ini` (`RequiredHack=` lines), `CustomFiles/` (overrides game paths), per-hack config files,
optionally Lua (`CustomFiles.lua`, `CustomFiles.ini` path handlers). Distributed as `.lmlm`: not a zip but an "LSPA"
package (512-byte sectors, one field per sector; format notes in `mod/tools/lmlm_extract.py`).
`port/compat/shar_mods.cpp` loads the mods in `SHAR_MODS` (':'-separated, earlier wins; `play.sh --mod`), each a
.lmlm read in place or an extracted folder, and reports hacks it ignores. Every read-only open goes through
`shar_ModResolve` once (`requests.cpp`, handed to `linuxdrive.cpp` via `shar_ModTakeResolved`; `binkffmpeg.cpp` for
movies): Lua path handlers, then CustomFiles.ini rules (OccludedPath, PathRedirections/AdditionalFiles), then
CustomFiles. Results: a mod file (a packaged one is an fd on the .lmlm plus a per-fd offset/size slice), generated
Lua output (a memfd), a redirect to another game path, or missing. The sound system opens files by key string
(decimal radMakeCaseInsensitiveKey32 of the backslashed path, as RCFs resolve them): files and rules are indexed by
that key too. Lua 5.4.8 is vendored in `port/third_party/lua` (one sandboxed state per mod, launcher API subset and a
virtual file system /Mods, /GameData, /GameDir in `shar_modlua.cpp`). `SHAR_MOD_LOG=1` logs each file a mod provides,
`SHAR_MOD_DUMP=dir` saves Lua-generated files. Main mods (`Main=1`) save to ModSaves/<InternalName>/; settings are
Meta.ini defaults unless ModSettings.ini in the game dir sets them.
Hacks implemented: CustomFiles, LuaSupport, OggVorbisSupport (x.ogg serves x.rsd; `radsound/linux/oggdecoder.cpp`,
FFmpeg, detected by the "OggS" header in `rsdfiledatasource.cpp`), CustomLimits [Billboards], [Regions] Limit /
EntityLimit, [Scripting] ArgumentLengthLimit (console arguments were unbounded copies). Movies up to 1920x1080
(64 tiles) and fitted to the window either way (IncreasedVideoResolutionSupport).
Config files reach the game through `shar_ModConfig` / `shar_ModConfigFolder`; XML ones through `shar_modxml.h`.
CustomShopSupport (only <CarShop>: the level's seller and conversation, `actionbuttonhandler.cpp`; PhoneBooth /
SkinShop item lists not yet), CustomDialogueCharacterCodes (only [CharacterCodes]: speaker codes appended to
`dialogline.cpp`'s characterTable; the outfit voice sections not yet), CustomText (`guitextbible.cpp`, through a
hook in Scrooby's FeLanguage::GetString; global/level/mission sections, $(Variables), \\ = \; English file only, no
Language setting yet), 3DPhoneBoothPreviewSupport (`guiscreenphonebooth.cpp`: the car shop's 3D preview path;
DarkenLockedCars/FullScreen not yet).
Fixed on the way (would also hit vanilla data in other cases): GL prim buffers drew all allocated indices instead
of the count set (uninitialised indices: intermittent crashes inside the GL driver with mod meshes), stream writes
past the allocation; null merchandise in `CharacterSheetManager::UpdateRewardsManager`; region list overflow.
Tested 2026-10-15: "SHAR Remastered" 1.0; Futurama: Hit & Run demo 0.1.3 boots (its movies, menu scene) and reaches
Level 1 by New Game (5/5 runs) and `skipfe`. Its other 20 hacks are not implemented (custom cars/characters/shops/
interiors/text, ASF commands, ...), nor its required HoverCarRefraction mod: driving and missions untested.
Docs: docs.donutteam.com/docs/LucasSimpsonsHitAndRunModLauncher/.

## Priorities (2026-10-17)

The base game is feature-complete but not yet verified end to end. In order:

1. **Verify the base game** (user plays; Claude does not script gameplay it can't see): a full playthrough of all
   seven levels. Riskiest first: save -> quit -> load round trip (saving from a `skipfe` session works since
   2026-10-17: on PC only the front end's memory card check selected the save drive), the bonus game (Itchy &
   Scratchy racing) with 2-4 pads, street/wager races and time trials, later levels' missions and cutscenes, French/German/Spanish.
2. **Memcheck coverage** (Claude, unattended; each pass has found real bugs): time trial / wager race, other interiors
   (Kwik-E-Mart, school, ...), later levels, the bonus game. See item 8 below.
3. **Robustness found along the way**: crashes/asserts from those runs, shutdown paths (window closed in any
   context), missing-data tolerance where cheap. Seen once (2026-10-16, not reproduced): quitting during a level
   load spun in `LoadingManager::CancelPendingRequests` waiting for the current load. Fixed 2026-10-17: shutting
   down with a vehicle sound playing overflowed the stack (`~PositionalSoundPlayer` deleted its owner).
4. **Mods, small to large**: CustomShopSupport item lists (PhoneBooth/SkinShop Free/Included/ExcludedItems),
   3D phone booth DarkenLockedCars, CustomDialogueCharacterCodes outfit voices, CustomText Language setting; then
   CustomCarSupport (unblocks the Futurama demo's cars), custom characters, interiors, audio, traffic and the rest
   of the hacks Futurama lists at startup. Test with the user in the loop (shops, phone booth, dialogue).
5. **Packaging**: install script or AppImage-style bundle (32-bit runtime libraries, FFmpeg, SDL2).
6. **Steering-wheel force feedback**: only with a wheel to test on.
7. **Modern OpenGL** (in progress on `dev`): core profile, D3D8 material parity and native DXT done (see lessons);
   next custom shader support (`pglDevice::AddCustomShader` is the pddi's hook, still empty).
8. **Long term**: 64-bit build (pointer/int casts, fixed-size heaps), a possible Rust/Bevy reimplementation
   (`RUST.md`).

## Known issues / next steps

1. (fixed 2026-10-13) 2D sprites upside down / HUD portraits looking clipped: DXT row order, see lessons.
2. (done 2026-10-13) Audio: SDL mixer in `linuxhal.cpp`. The mixer owns voice play positions; without a device
   (or `SHAR_NO_AUDIO=1`) voices advance on a timer. EAX reverb (2026-10-15, verified by ear by the user): software
   EAX2-style reverb in the mixer (`radSoundLinuxReverb`: room HF, early taps, 8 damped combs + 4 allpasses per side).
   As under DirectSound only positional voices feed it; ReverbController's presets (scripts.rcf `reverbSettings`:
   tunnels, sewers, mansion, power-plant rooms, ...) arrive through the effect's setters, its fades through the
   system aux gain (which DirectSound's HAL ignored). `SHAR_REVERB=0` off, `SHAR_REVERB_LOG=1` logs presets/fades.
   Test without speakers: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` (float32 stereo, 48 kHz).
3. (done 2026-10-13) Movies: Bink via FFmpeg, see platform model. Clock is wall time; audio decoded ~0.5 s ahead.
4. (done 2026-10-15) Gamepad rumble: `sdlcontroller.cpp` gives pads the "LeftMotor"/"RightMotor" output points
   `RumbleEffect` wants (DirectInput never had them: retail PC gamepads never rumbled), sent with
   SDL_GameControllerRumble; gated by the Force Feedback option (`useforcefeedback`, default off).
   `SHAR_RUMBLE_LOG=1`. Verified on a PlayStation pad by the user. Steering-wheel effects not done.
5. (done 2026-10-14) Input verified in play by the user: keyboard, mouse, wired and wireless controllers.
   Gamepad Start pauses/resumes (new front-end input `feStart` on button 7; Win32 paused only with Escape).
   Linux gamepad defaults (`UserController::LoadDefaults`) follow the console layout in SDL's button order (the
   DirectInput defaults put Sprint/Accelerate on Start): the user's own PS4 mapping.
   Fixed 2026-10-17: SDL sends SDL_CONTROLLERDEVICEADDED for pads already present at startup too, so a plugged-in pad
   was opened twice (Joystick0 and Joystick1); the front end listens to every controller and moved two menu items per
   press (in-game menus read only player 1). `OpenGameController` now skips a device already open.
6. (done 2026-10-14) Release build used throughout testing. 64-bit later (a -m64 syntax survey found ~80
   pointer-to-int casts, ~35 int-to-pointer casts; fixed-size game heaps would need enlarging); modern GL later.
7. (done 2026-10-14) Missing fences/ramps in Level 7: alphaRef fix above; HUD and level transparency confirmed
   in play by the user.
8. (done 2026-10-14) Uninitialised-read hunt with memcheck: boot, Level 1, walking outdoors and the first mission
   are clean (Character parent transforms/ground data, InteriorManager gag flag, TrafficAI state, Context state,
   sim joint state and cloned articulated objects, DSG rank/geo flags). Level 1 now starts identically with
   `SHAR_FILL_ALLOC=0x00` and `0xFF`. 2026-10-15: Level 7 Mission 1 (`skipfe l7 m1`, starts in the Simpsons' house):
   interior, talking to Lisa, leaving the house and walking outdoors are clean after one fix (MissionObjective
   `PathStruct::mUpdateCount`, read before a path is lit). Under memcheck L7 takes 3-5 min to load; start key
   scripts at ~330 s. Driving (2026-10-15): `port/memcheck.sh 900 skipfe demo l1` (AI drives, no input needed) is
   clean after fixes to Avatar race data and Character::mbNeedChoreoUpdate; it also found GetTotalMemoryUsed()
   with no return on Linux (GCC traps: SIGILL when the AI reached its last waypoint; a -Wreturn-type scan of every
   source found no other). Under memcheck the world streams in late, so the car often drives over unloaded zones.
   2026-10-16: memcheck.sh now runs valgrind with --fair-sched=yes (without it the main/load threads' mutex
   ping-pong starved the drive thread: Level 1 took 15+ min to load, now ~3) and driving runs use
   SHAR_MAX_FRAME_MS=33 (else the car outruns streaming onto empty ground; `DSG: Move: can't find` lines came from
   that). Clean: player driving in Level 1 (`skipfe incar`), `demo l2` and `demo l3` after two fixes
   (RenderManager::mbFirstDynamicZone, ParkedCarManager::RemoveFreeCarIfClose position).
   2026-10-17: Level 1 races 2 (circuit) and 3 (waypoint) (`port/memcheck.sh 1200 skipfe race2`, Enter taps from
   120 s, W held, SHAR_MAX_FRAME_MS=33) clean after two fixes (MouseCursor position, AnimIconDSG::m_Visible). Long key
   scripts used to starve loading under valgrind (each keyboard poll re-parsed the script per key; now parsed once).
   Not yet covered: the time trial and wager race, other interiors (Kwik-E-Mart, school, ...), the bonus game.
9. (done 2026-10-14) Widescreen verified by the user: scales correctly at all ratios, including extreme ones.
   Borderless fullscreen and the runtime resolution list added the same day.
10. (in progress 2026-10-15) Mods: Futurama: Hit & Run demo 0.1.3 drives in Level 1; the car shop (seller,
    conversation, names), clothes store names and the 3D phone booth work (user-tested). Still unimplemented: custom
    cars/characters/interiors/audio/traffic, ASF commands, HoverCarRefraction and the rest of the 17 listed at startup.
