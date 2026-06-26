# CLAUDE.md: The Simpsons Hit & Run, native Linux port

Port of the original 2003 source to 32-bit Linux with SDL2 + OpenGL. The untouched original dump is at
`~/Documents/Decomp/hitnrun_source/game` (never modify it). This repo holds only the files the build uses
(sources, included headers, the VS project files `gen_cmake.py` reads, and the Win32 originals it swaps for Linux ones).
Status (2026-10-12): builds, links, boots, loads Level 1 and renders gameplay with correct colours.
Sound plays through an SDL2 software mixer (no reverb); Bink movies play via FFmpeg; force feedback is absent.

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
  compat/                  force-included msvc_compat.h, types-only windows.h, crtdbg.h, dinput.h,
                           win32_threads.* (threads/sync/fibers/timers on POSIX), win32_misc.* (MessageBox),
                           pddi_heap.cpp (renderer's private heap)
build-linux/               release build (RAD_RELEASE)     cmake -B build-linux -G Ninja
build-linux-debug/         debug build (RAD_DEBUG: asserts + Radical's debug output)   -DSHAR_RAD_DEBUG=ON
build-linux-audit/         -DSHAR_UNINIT_AUDIT=ON -DCMAKE_BUILD_TYPE=Release: all warnings on; grep -Wuninitialized /
                           -Wmaybe-uninitialized (2 left 2026-10-13, both false positives: actordsg.cpp, opticlensflare.cpp)
run/                      run dir (git-ignored): symlinks to hitnrun_source/game/cd/PC data, private simpsons.ini/Save1-4
```

## Commands

```bash
python3 port/gen_cmake.py                 # after changing port rules; then ninja regenerates
ninja -C build-linux-debug -j6
SHAR_BUILD=$PWD/build-linux-debug port/run.sh 120      # absolute SHAR_BUILD path
SHAR_SHOT_EVERY=10 ...                    # renderer saves shot_NNN.png every 10 s in the run dir
SHAR_ASSERT_ABORT=1 ...                   # make failed asserts abort instead of log-and-ignore
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 MESA_DEBUG=1 ...   # Mesa reports GL errors
python3 port/syntax_survey.py; python3 port/check.py code/foo.cpp
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
  rerun the audit build after porting new code. Input debug env vars: `SHAR_INPUT_LOG`, `SHAR_NO_INPUT`,
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
- Boot movies only play in release builds (`SHOW_MOVIES` needs `RAD_RELEASE`, `bootupcontext.cpp`): test them with
  `build-linux`, not the debug build.
- radcore's `radoptions.hpp` enables debug comm / watcher / memory monitor in debug builds: off on Linux.

## Known issues / next steps

1. (fixed 2026-10-13) 2D sprites upside down / HUD portraits looking clipped: DXT row order, see lessons.
2. (done 2026-10-13) Audio: SDL mixer in `linuxhal.cpp`. The mixer owns voice play positions; without a device
   (or `SHAR_NO_AUDIO=1`) voices advance on a timer. Not done: EAX reverb (aux sends ignored).
   Test without speakers: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` (float32 stereo, 48 kHz).
3. (done 2026-10-13) Movies: Bink via FFmpeg, see platform model. Clock is wall time; audio decoded ~0.5 s ahead.
4. Force feedback via SDL haptics/rumble (controller output points).
5. Verify input in play (keyboard DIK mapping, mouse look via relative mode, gamepads).
6. Release build (`build-linux`) should be re-tested; 64-bit later; modern GL later.
