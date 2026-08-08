# Post-processing shaders

GLSL fragment shaders that run over the picture each frame: anti-aliasing, bloom, colour grading, a CRT look, or your
own. They are plain text files; edit them while the game runs and the change shows up within a second.

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

Settings are best written as `const float NAME = value;` at the top of the file (the examples do that). A file
that does not compile is reported on the console with its line numbers (in the log, `run/logs/last-session.log`,
when started by play.sh) and skipped until it builds again; the other passes keep running.

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
