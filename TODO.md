# Progress

What the port does so far and what is left. User-facing overview: [`README.md`](README.md); porting details:
[`CLAUDE.md`](CLAUDE.md).

## Done

### Build and platform
- [x] **Build system**: CMake + Ninja project generated from the original Visual Studio projects
  (`port/gen_cmake.py`), 32-bit (`-m32`), release and debug (original `RAD_DEBUG`) configurations
- [x] **Platform layer**: SDL2 window and event loop; Win32 threads, sync, fibers and timers on POSIX;
  case-insensitive file access to the game data
- [x] **Launcher**: `port/play.sh` builds if needed, sets up the run directory and starts the game
- [x] **Crash reports**: each session logged; crashes kept with the backtrace resolved to source lines
- [x] **Closing the window during boot or loading**: no longer hangs or crashes (sound loads, file errors at unmount,
  radscript teardown, vehicles destroyed after their managers, load callbacks while cancelling)
- [x] **Missing files**: reported (`File not found: …`) instead of hanging behind the original retry screen
- [x] **Self-contained Linux build**: `port/package.sh` bundles the game with its 32-bit libraries and a launcher

### Renderer
- [x] OpenGL Pure3D backend on SDL2
- [x] **OpenGL 3.3 core profile**: GLSL shaders reproduce the Direct3D 8 renderer's materials and lighting (car
  paint reflections, two-sided materials, fading cars and characters); geometry in GPU buffers
- [x] DXT textures stay compressed on the GPU (decoded in software on drivers without S3TC)
- [x] Multithreaded loading via shared GL contexts (Direct3D 8 was multithreaded, GL is not)
- [x] Texture clamping matching Direct3D (no dark seams)
- [x] Vsync plus a 60 fps cap (physics and AI break at thousands of fps)
- [x] Colour-write masks and alpha testing matching Direct3D (HUD map, see-through fences and ramps)
- [x] Gamma applied by a shader pass (works in a window and under Wayland)
- [x] Multisample anti-aliasing (4x by default) with alpha-to-coverage for cut-out textures
- [x] Mipmaps with up to 16x anisotropic filtering
- [x] User post-processing shaders (ambient occlusion, bloom, colour grading, tone mapping, CRT, ...)
- [x] User material shaders (toon shading, car paint, ...), by material type, file or name; mods can ship them

### Display
- [x] **Any aspect ratio**: Hor+ 3D view, centred 4:3 UI; letterbox bars, fades and the front-end TV frame span
  the window; verified in play, including extreme ratios
- [x] **Resizable window**: adapts live to any size
- [x] **Fullscreen**: borderless at the desktop resolution, or exclusive at the chosen resolution
- [x] **Resolution menu**: common PC sizes plus the desktop's own, filtered to what fits; any `WxH` from
  `simpsons.ini` accepted

### Input
- [x] Keyboard and mouse (mouse look via relative mode), wired and wireless gamepads through SDL; verified in play
- [x] Start pauses; the menu cursor hides while a gamepad is in use
- [x] Gamepad rumble (the Windows release never rumbled gamepads)

### Sound and video
- [x] Software mixer on SDL audio with 3D positioning: sound effects, music, dialogue
- [x] EAX reverb in software, driven by the game's own presets; verified by ear
- [x] Bink movies decoded with FFmpeg, audio through the game's mixer

### Game data and gameplay
- [x] **Retail PC data**: split music archives (`music00`–`03.rcf`), Radical ADPCM dialogue, logo movies replaced
  by stubs in some copies
- [x] **Gameplay**: loads levels and plays, tested into Level 7
- [x] **Uninitialised reads** that were zero by luck under MSVC: fixed where found
- [x] **Saving from a `skipfe` session**
- [x] **Save, quit, load** round trip from a normal game; verified in play
- [x] **Test launch options**: `skipfe`, `skipfe l<N> m<N>`, `demo l<N>`, `incar`, `tp<N>`, `race<N>`, `coins`

### Memory checking (valgrind memcheck, clean after fixes)
- [x] Boot and the front end
- [x] Level 1: walking, the first mission, player driving
- [x] Level 1 races: time trial (including its restart), circuit, waypoint, wager
- [x] Interiors: Kwik-E-Mart, school, Simpsons' house and Bart's room, Moe's, DMV, Android's Dungeon, Observatory
- [x] Level 7 Mission 1 (Simpsons' house, talking to Lisa, walking outdoors)
- [x] Attract demos (AI driving) of all seven levels

### Mods (Lucas' Mod Launcher)
- [x] `.lmlm` packages and extracted folders, several at once
- [x] CustomFiles, LuaSupport, OggVorbisSupport
- [x] CustomLimits: every section except [Sound] (its defaults match nothing in this source; see CLAUDE.md)
- [x] CustomInteriorSupport, CustomRoadBehaviour (DisableTrafficGroundSnapping), DynamicTreeNodeEntityLimits
- [x] Fully Connected Map 1.0.7: Level 1 plays, interiors entered (Kwik-E-Mart, Moe's)
- [x] CustomLicenseScreenTime (Time, Skippable), NoMainMenuCameraAnimations, HUDMapIgnorePlayerHeight
- [x] IncreasedVideoResolutionSupport (movies up to 1920x1080)
- [x] CustomText (with the Language setting)
- [x] CustomShopSupport: car shop seller and conversation; phone booth and skin shop item lists and icons
- [x] CustomDialogueCharacterCodes: new speaker codes, outfit voices ([NongenericOutfits]/[NongenericCharacters])
- [x] 3DPhoneBoothPreviewSupport (all settings)
- [x] AdditionalScriptFunctionality: what the Futurama demo uses (see To do for the rest)
- [x] CustomCarSupport: every setting ([FakeWheel] CopySkidMarks read, not drawn)
- [x] Futurama: Hit & Run demo plays Level 1 with its shops and phone booth; missions m0-m4 run through (autopass)

## To do

### Verification (playing the game)
- [ ] **Full playthrough** of all seven levels: missions and cutscenes (in progress: Levels 1-2 done, smooth; on Level 3)
- [ ] **Bonus game** (Itchy & Scratchy racing) with 2-4 pads
- [ ] **Street races, wager races and time trials** in every level
- [ ] **French, German and Spanish**

### Memory checking
- [x] The bonus game (2026-10-20, 64-bit: menus, a 3-lap AI race, standings; one fix: WorldScene view vector)
- [x] Later levels' missions: one per level, every stage via `autopass` (2026-10-20; 5 fixes)
- [ ] The other missions of each level (L1, L2, L4 M2-7 and L3 M2-5 done 2026-10-20, 1 fix + 2 crashes fixed;
  L3 M6-7, L5-L7 and L7 M2 left)

### Robustness
- [ ] Shutdown paths: closing the window in every context. Release and debug builds swept with `SHAR_QUIT_AT` (boot,
  front end, Level 1/7 load (100 ms steps across its end) and gameplay, pause, interior, car, race, demo): no hangs or
  crashes left; still to do: the bonus game, missions in progress, cutscenes, the front end's Quit, quitting while saving
- [x] Debug-build teardown: no crashes; the asserts left come from the original teardown order (see CLAUDE.md)
- [ ] Missing-data tolerance where it is cheap (`art\wrench.p3d` missing still crashes later)

### Mods
Order of attack: CustomTrafficSupport first (it also completes ASF's traffic-group commands), then CustomAudioSupport
and the character/animation hacks (what the larger total conversions use); the small cosmetic hacks are quick wins
in between. Hacks a mod requires but the port lacks are listed at startup (`mods:   not supported yet, ignored: ...`).

#### Hacks not implemented (all required by the Futurama demo)
- [ ] **CustomTrafficSupport**: a mod's own traffic cars (ASF's traffic-group commands wait for it)
- [ ] **CustomAudioSupport**: new sounds and music
- [ ] **CustomAnimationSetSupport**: animation sets for custom characters
- [ ] **CustomBonusMissionSupport**: bonus missions beyond the game's own
- [ ] **CustomTriggerActions**: new trigger volume actions
- [ ] **CustomSkinPreviewAnimations**: animations in the clothes-shop preview
- [ ] **CustomStatsTotals**: the stats screen's totals (cards, wasps, gags, ...)
- [ ] **OverrideShaderParameters**: per-material shader parameters
- [ ] **HoverCarRefraction**: a mod (not a hack) the Futurama demo lists as required
- [ ] The Mod Launcher's other hacks, none of the three test mods uses: survey its hack list for popular mods

#### Partly done
- [ ] **AdditionalScriptFunctionality** (`code/mission/modscript.cpp`): every command registered (unimplemented ones log
  once when used). Done: the objective/condition types except insidetrigger/outsidetrigger, the If* conditionals,
  stage payout / Hit & Run / ped group / music / cancel settings, DisableTrigger, checkpoints, passengers
  (AddStageVehicleCharacter). Left:
  - [ ] passengers: animation, scale, visibility, jumping out
  - [ ] stage vehicle settings, SetStageCharacterModel
  - [ ] forces and gravity
  - [ ] stage dyna load data
  - [ ] traffic groups (need CustomTrafficSupport)
  - [ ] parked car / pedestrian switches
  - [ ] SetObjSound / SetCondSound, explosions, SetObjUseCameraPosition
  - [ ] insidetrigger / outsidetrigger objective and condition types
  - [ ] CON commands (SetConditionalParameter, SetWheelieOffsetX)
- [ ] **CustomLimits [Sound]** PlayingClipPlayerLimit / PlayingStreamPlayerLimit: the docs' defaults (25, 8) match
  nothing in this source (64 clip, 9 stream players); needs the retail behaviour. Values up to 64/9 fit already
- [ ] **CustomRoadBehaviour UTurnTolerance**: undocumented, logged and ignored
- [ ] **CustomCarSupport [FakeWheel] CopySkidMarks**: read, not drawn (the skid mark generator has four wheels)
- [ ] **Material shaders** from more than the first mod's `Shaders/` folder

#### Checks in play (so far only automated runs and screenshots)
- [ ] **Futurama** Level 1 missions by hand: m1-m4 and bm1 (kicking the pawn in m2, the wasps in m3, the speed
  objective and the speed-range bar)
- [ ] **Custom cars**: a destroyed car leaving a custom husk (Husk=), PreviewScale, IsHusk
- [ ] **Fully Connected Map**: the Level 2/3 areas inside its Level 1, the DMV, Android's Dungeon and Observatory
  interiors (doors listed in CLAUDE.md), traffic on its bridges (Road006/Road0061), its warp missions
- [ ] **Raised CustomLimits** pushed past the game's own values in play (cars, stages, trigger volumes, ...): so far
  only checked that a mod raising them all loads and quits cleanly

### Platforms
- [x] **64-bit build** boots and plays: movies, front end, Levels 1/7, races, interiors, driving, sound; AddressSanitizer clean
- [ ] **64-bit verification**: long play sessions. Done: the bonus game (ASan clean, quits in its menu/load/race), mods under
  ASan, memcheck tooling (Level 1 clean), packaged bundle (`port/package.sh --64`)
- [ ] **Windows x64**
- [ ] **macOS** (Apple silicon and Intel)
- [ ] **Packaging**: an AppImage-style bundle per platform (Linux has a tarball from `port/package.sh`)

### Renderer extras
- [ ] Anti-aliasing entry in the display menu
- [ ] Half-resolution post-processing passes
- [ ] Sun lighting on the world (needs shadow maps; prototyped and dropped)
- [ ] **Vulkan renderer** (shelved 2026-10-19): a third pddi beside GL, GL kept as fallback. Reuses the GL
  pipeline's CPU state tracking and GLSL (compiled to SPIR-V with glslang); new work is pipeline objects/cache,
  memory and frame sync, swapchain, texture Lock/readbacks. For Android, macOS (MoltenVK), low-power devices,
  loading threads without the shared GL context pool, ray-traced shadows, DLSS/FSR. Not for desktop speed.
  Cheaper alternative to evaluate first: `pddidx8` on DXVK-native's D3D8 (loses the GL extras). Start after 64-bit
  is the default, with the legal screen and front end compared to GL by screenshot

### Other
- [ ] **Steering-wheel force feedback**: springs and dampers; needs a wheel to test with
