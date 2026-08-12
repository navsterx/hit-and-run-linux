# Shaders

Two kinds of GLSL shaders of your own, both plain text files you can edit while the game runs (changes show up
within a second):

- **Post-processing** (`post.ini`, `post/`): passes over the picture each frame: anti-aliasing, bloom, colour
  grading, a CRT look, ...
- **Material shaders** (`materials.ini`, `materials/`): replace how chosen materials are drawn: toon shading, car
  paint, ... (see [Material shaders](#material-shaders) below)

A shader that does not compile is reported on the console with its line numbers (in the log,
`run/logs/last-session.log`, when started by play.sh) and the game carries on without it until it builds again.

# Post-processing shaders

## Where they go

The game reads the `shaders/` folder in its working directory (or the folder named by the `SHAR_SHADERS`
environment variable). `port/play.sh` sets it up: your game folder's `shaders/` if it has one, otherwise a copy of
this folder in `run/shaders/` (edit that copy, not this one). `SHAR_POST=0` turns post-processing off.

## Choosing passes: post.ini

```ini
[scene]                  # see the 3D world, before the HUD and menus are drawn on top
post/bloom_bright.glsl
post/bloom_blur_h.glsl
post/bloom_blur_v.glsl
post/bloom_add.glsl
post/fxaa.glsl

[final]                  # see the whole frame, HUD and menus included
post/crt.glsl
```

Each line names a shader file, relative to the shaders folder. Passes run top to bottom, each one reading the
previous one's output. `#` or `;` starts a comment. Up to 16 passes per stage. The game's gamma setting is applied
after everything. The scene stage only runs when the 3D world was drawn (not in the front-end menus).

## Writing a pass

A pass is the body of a GLSL 3.30 fragment shader, without a `#version` line. The game puts this in front of it:

```glsl
in vec2 vUV;                    // position on the screen: (0,0) bottom left, (1,1) top right
out vec4 fragColour;            // write the pixel's colour here
uniform sampler2D uScene;       // the picture so far (the previous pass's output)
uniform sampler2D uOriginal;    // the picture as this stage began (before its first pass)
uniform sampler2D uDepth;       // the depth buffer (0 near .. 1 far, not linear)
uniform vec2 uResolution;       // the picture's size in pixels
uniform float uTime;            // seconds since the game started
uniform int uFrame;             // frame counter
uniform float uNear, uFar;      // the camera's near and far planes (0 when the last view was not 3D)
float LinearDepth(vec2 uv);     // distance from the camera at uv, in game units (about metres)
```

The smallest pass:

```glsl
void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    fragColour = vec4(1.0 - c, 1.0);        // negative image
}
```

Settings are best written as `const float NAME = value;` at the top of the file (the examples do that).

`uDepth` is copied only when a pass uses it. In the final stage it also holds what the HUD and menus drew.

## The examples (post/)

| File | Stage | What it does |
|------|-------|--------------|
| `fxaa.glsl` | scene | anti-aliasing: smooths jagged edges |
| `sharpen.glsl` | scene | contrast-adaptive sharpening |
| `grade.glsl` | scene | exposure, contrast, saturation, warmth |
| `vignette.glsl` | scene | darker corners |
| `bloom_bright.glsl`, `bloom_blur_h.glsl`, `bloom_blur_v.glsl`, `bloom_add.glsl` | scene | glow around bright things (four passes, in this order, first in their stage) |
| `crt.glsl` | final | old TV: curved screen, scanlines, shadow mask |
| `show_depth.glsl` | scene | shows the depth buffer (near black, far white) |

Cost: all of them together take about 0.25 ms a frame at 1280x720 on a current graphics card.

# Material shaders

A material shader takes over the colour of the materials it is assigned to. The game still does the rest: it
positions and lights the vertices as before, and fog, the alpha test (see-through parts) and anti-aliasing are
applied to what the shader returns.

## Assigning them: materials.ini

```ini
[types]                     # by the shader type the game's art names
spheremap = materials/carpaint.glsl
[files]                     # by the art file the material comes from (wins over [types])
*chars/* = materials/flat.glsl
[materials]                 # by material name (wins over [files] and [types])
homer_v* = materials/toon.glsl
```

Each line is `pattern = file`. Patterns may use `*` and `?` and ignore case; the file is relative to this folder.
Within each section the first matching line wins.

- **Types**: `simple` is almost everything, including the HUD and menus; `spheremap` and `environment` are car
  paint and chrome. A mod's art can name a type of its own (`toon`, say), which draws as `simple` unless a rule
  gives it a shader.
- **Files** are the art files materials are loaded from, as the game opens them (`art/chars/homer_m.p3d`; the
  case varies, matching ignores it): `*chars/*` is every character, `*cars/*` every car, `*cars/famil_v*` one car.
- **Material names** come from the art. Start the game with `SHAR_MATERIAL_LOG=1` to print each material's name,
  type and file the first time it is drawn.

**Mods** can ship material shaders: a `Shaders/` folder in the mod's root folder holding a `materials.ini` and the
shader files it names, next to it (no subfolders). Your own `materials.ini` comes first, so its rules win over a
mod's.

## Writing one

A material shader is the body of a GLSL 3.30 fragment shader, without a `#version` line, that defines
`vec4 ShadeMaterial()` and returns the colour (alpha included). The smallest one draws everything as the game would:

```glsl
vec4 ShadeMaterial()
{
    return StandardColour();
}
```

Available to it:

```glsl
vec4 StandardColour();          // the material as the game draws it (lit colour x texture + reflection + highlight)
vec4 BaseTexture();             // the material's texture at this pixel (white without one)
vec3 LightVector(int i, out float attenuation);  // towards light i from this pixel, eye space; attenuation 0 out of range

in vec4 vColour;                // the lit colour (unlit materials: their vertex colours, which hold baked lighting)
in vec3 vSpecular;              // the game's highlight
in vec2 vUV;                    // texture coordinates
in vec2 vReflectionUV;          // reflection map coordinates (car paint)
in vec3 vNormal;                // the normal, eye space (x right, y up, z towards the camera)
in vec3 vEyePosition;           // the pixel's position, eye space (the camera at 0,0,0)

uniform sampler2D uTexture;            uniform bool uTextured;
uniform sampler2D uReflection;         uniform bool uReflectionTextured;   uniform vec4 uEnvBlend;
uniform bool uLit;                     // the material is lit by the game's lights
uniform vec4 uMatAmbient, uMatDiffuse, uMatSpecular, uMatEmissive;   uniform float uShininess;
uniform vec4 uGlobalAmbient;
uniform int uLightCount;               uniform vec3 uLightColour[8];   // the lights in use
uniform float uTime;                   // seconds since the game started
```

(`gl_FrontFacing` is false on the side of a polygon the game shows: the renderer mirrors the scene in depth.)

Most of the world is unlit: its lighting is painted into the vertex colours and it has no normals (`vNormal` is
then the same across each object), so lighting-based effects work on lit materials (characters, cars). `StandardColour()`
and `vColour` are the way to include the world's own shading.

## The examples (materials/)

| File | For | What it does |
|------|-----|--------------|
| `flat.glsl` | `[files] *chars/*` | characters in flat cartoon colours: no shading across the model, still as bright or dark as the scene |
| `toon.glsl` | `simple` | cel shading: light in flat bands (per-pixel on lit materials; colours banded on the rest) |
| `carpaint.glsl` | `spheremap`, `environment` | the car paint plus a sharp highlight per light and stronger reflections at grazing angles |
| `normals.glsl` | anything | shows the normals as colours |

They cost nothing measurable on a current graphics card.
