# Rust + Bevy reimplementation: planning notes

Notes for a possible future project: a new engine for *The Simpsons: Hit & Run* in Rust on Bevy, loading the
original game data. Nothing here is started; this C++ port comes first.

## The approach: reimplement the engine, don't translate the code

The source is about 670,000 lines of C++:

| Part | Lines | Bevy replacement? |
|---|---|---|
| `code/` (the game) | ~380,000 | Rewritten as Bevy systems |
| `libs/pure3d` (renderer, formats) | ~92,000 | Bevy rendering; format loaders rewritten |
| `libs/radcore` (files, threads, memory, input) | ~76,000 | Rust std + Bevy |
| `libs/sim` (physics) | ~25,000 | Rapier (or custom), tuned to match |
| `libs/choreo`, `libs/poser` (character animation) | ~25,500 | Bevy animation + custom blending |
| `libs/scrooby` (UI) | ~24,000 | Page loader drawn with Bevy UI/sprites |
| `libs/radmusic`, `libs/radsound` (music, sound) | ~34,500 | bevy_audio / bevy_kira_audio + custom |

A line-by-line translation is not realistic. The workable model is an **engine reimplementation**, as done by
OpenMW (Morrowind) and OpenRCT2: a new engine that reads the player's own copy of the game data and reproduces
its behaviour.

## Legal: decide this first

This C++ repository must stay private because it is the leaked source. A reimplementation that loads data from
the player's own copy *could* be public, but only if it is not a translation of the leaked code. Porting the C++
to Rust line by line would most likely still be a derivative work. Options:

- **Clean room:** one person reads the source and writes specifications (formats, rules, constants); someone
  else writes the code from those specs only.
- **Reference only:** use the source to understand file formats and behaviour, observed through this C++ port
  as a running reference, and never copy or transliterate code.

Not legal advice; the point is that this is far harder to fix after code has been written.

## How this C++ port helps

- **Running reference.** Run any scene here, take screenshots and log values, compare with the Bevy version.
  Useful switches: `SHAR_KEY_SCRIPT` (scripted input), `SHAR_SHOT_EVERY` (screenshots),
  `SHAR_RESIZE_SCRIPT`, `skipfe` (straight into Level 1). See `README.md`.
- **Format knowledge** (below), confirmed against real data during this port.
- **Lessons learned** (`CLAUDE.md`) that apply to any engine running this data: DXT row order, 4:3 design
  assumptions, physics/AI breaking at very high or uneven frame rates, uninitialised values that MSVC
  happened to zero.

## File formats

### RCF archives (`*.rcf`): "cement libraries"

- Signature: starts with `RADCORE CEMENT LIBRARY`.
- Contains a table of `(hash, offset, size)` entries sorted by hash (binary searched), and a name table.
- Files are looked up by hashing the path case-insensitively (`radMakeCaseInsensitiveKey32`,
  `libs/radcore/inc/radkey.hpp:184`). If that misses, a numeric file name is parsed with `atoi` and used as the
  key directly (`libs/radcore/src/radfile/common/cementer.cpp`): the game opens many sound files by number.
- Names in the name table are relative (e.g. `agnes\p_hitbyc_agn_hurt.rsd` in `dialog.rcf`).
- Reader: `libs/radcore/src/radfile/common/cementer.cpp`, `cementLibrary.hpp`.

Retail differences: music is split into `music00`–`music03.rcf` (the leaked data has one `music.rcf`); some
copies replace the logo movies with tiny Ogg files.

### RSD audio (`*.rsd`)

- Header: `RSD4` + 4-character encoding (`PCM `, `RADP`; consoles also `VAG `, `XADP`, `GADP`), then
  little-endian `u32` channels (offset 8), bits per sample (12), sample rate (16). Usually 24000 Hz.
- Sample data starts at offset 2048.
- Header struct: `radSoundHalFileHeader`, `libs/radsound/inc/radsoundfile.hpp`.

**RADP (Radical ADPCM):** used by the retail `dialog.rcf` (mono, 24 kHz). Standard IMA ADPCM step and index
tables. Per block and channel: 20 bytes = `i16` step index + `i16` previous sample + 16 bytes of 4-bit codes
(low nibble first), giving 32 samples. Decoder: `libs/radsound/src/radsound/radicaladpcm.cpp`. Verified to
reproduce the PCM dialogue in the leaked data within normal ADPCM error.

### P3D (`*.p3d`): models, textures, animations, cameras, UI pages

- Chunk tree. The file starts with `0xFF443350` (`P3D\xff`; `0x5A443350` = compressed variant, byte-swapped
  IDs = other endianness).
- Every chunk: `u32 id`, `u32 dataLength` (header + own data), `u32 chunkLength` (including children);
  12-byte header. Children follow the chunk's own data.
- Chunk IDs: `libs/pure3d/constants/chunkids.hpp`, `chunks.h`. Reader: `libs/pure3d/p3d/chunkfile.cpp`; each
  object type has a loader in `libs/pure3d/p3d/` (meshes, textures, skeletons, animations, ...) and in
  `code/loading/` for game-specific chunks.
- Textures on PC are often DXT-compressed. Mind the row order: see "Texture row order" in `CLAUDE.md`.
- Scrooby UI pages are also P3D chunks (`art/frontend/scrooby/*.p3d`), laid out on a 640x480 page.

### Other data

- **Scripts** (`scripts/`, `scripts.rcf`): level and mission setup scripts, plain text commands.
- **Movies** (`movies/*.rmv`): Bink 1 video. FFmpeg decodes them (this port uses it); a Rust option is FFmpeg
  bindings or a native Bink 1 decoder.
- **Config/saves:** `simpsons.ini`, `Save1`–`Save4`.

## Suggested phases

1. **Format crate** (standalone Rust library, no Bevy): RCF reader, P3D chunk parser, RSD/RADP decoder, DXT
   decode. Test against real data; cross-check with this port. Useful on its own (asset browser, exporter).
2. **Level viewer:** Springfield's world geometry and textures in Bevy with a fly camera. First visible
   milestone; proves the P3D mesh/texture loaders.
3. **Characters and vehicles:** skinned meshes and animation (`choreo`/`poser` behaviour), then car physics.
   Physics is the hardest thing to make *feel* right.
4. **Gameplay systems:** cameras (the SuperCam family in `code/camera/`), on-foot and driving controls, traffic,
   pedestrians, collectibles, hit-and-run meter.
5. **Missions and scripts:** mission system, dialogue, cutscenes (NIS), conversations.
6. **UI and HUD:** Scrooby page loader and screens.
7. **Audio and music:** sound banks, positional audio, the interactive music system (`radmusic`).

## Notes on Bevy

- Bevy's ECS is very different from SHAR's class hierarchies; design the architecture fresh rather than
  mapping classes to components.
- Bevy makes breaking API changes between releases: pin a version, upgrade deliberately.
- Benefits: native 64-bit, modern rendering, any aspect ratio for free, and a much easier route to other
  platforms (including Android) than the C++ engine.

## Sizing

- Format crate and level viewer: a few weeks.
- Fully playable game: a multi-year effort, typically an open-source project with contributors.
