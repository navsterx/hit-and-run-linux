// Ambient occlusion (SSAO), pass 1 of 2: how hidden each point is by the geometry around it (corners, the ground
// under cars and around feet, where walls meet the ground), from the depth buffer. Outputs the occlusion as grey;
// ssao_apply.glsl smooths it and darkens the picture with it. Put both in [scene], in that order.

#pragma shar_original                  // the effect starts here: ssao_apply reads this pass's input as uOriginal

const int SAMPLES = 16;                 // more = smoother, slower
const float RADIUS = 0.8;               // how far around a point to look (game units, about metres)
const float BIAS = 0.04;                // ignore tiny depth differences (stops flat surfaces darkening themselves)
const float FADE_START = 40.0;          // occlusion fades out between these distances from the camera ...
const float FADE_END = 90.0;            // ... (far away the depth buffer is too coarse)

void main()
{
    float depth = texture(uDepth, vUV).r;
    if(uFar <= 0.0 || depth >= 1.0)
    {
        fragColour = vec4(1.0);         // no 3D view, or the sky
        return;
    }

    vec3 p = ViewPosition(vUV);
    float distance = -p.z;

    // the surface normal from the neighbouring pixels (the smaller step on each side avoids smearing over edges)
    vec2 pixel = 1.0 / uResolution;
    vec3 right = ViewPosition(vUV + vec2(pixel.x, 0.0)) - p;
    vec3 left = p - ViewPosition(vUV - vec2(pixel.x, 0.0));
    vec3 up = ViewPosition(vUV + vec2(0.0, pixel.y)) - p;
    vec3 down = p - ViewPosition(vUV - vec2(0.0, pixel.y));
    vec3 dx = abs(right.z) < abs(left.z) ? right : left;
    vec3 dy = abs(up.z) < abs(down.z) ? up : down;
    vec3 n = normalize(cross(dx, dy));
    if(dot(n, -p) < 0.0)
        n = -n;

    // a different rotation of the sample pattern per pixel (the apply pass blurs the noise away)
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec3 helper = abs(n.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(helper, n));
    vec3 b = cross(n, t);

    float occlusion = 0.0;
    for(int i = 0; i < SAMPLES; i++)
    {
        // points in the hemisphere above the surface, more of them close in
        float f = (float(i) + 0.5) / float(SAMPLES);
        float angle = float(i) * 2.39996 + noise * 6.28318;
        float cosTheta = sqrt(1.0 - f);
        float sinTheta = sqrt(f);
        vec3 direction = t * (cos(angle) * sinTheta) + b * (sin(angle) * sinTheta) + n * cosTheta;
        float scale = mix(0.15, 1.0, f * f) * RADIUS;
        vec3 s = p + direction * scale;

        // where that point lands on screen, and what is drawn there
        vec4 clip = uProjection * vec4(s, 1.0);
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            continue;
        float sceneZ = ViewPosition(uv).z;

        // occluded when something is in front of the sample point; not by things far in front (range check)
        float inFront = sceneZ >= s.z + BIAS ? 1.0 : 0.0;
        float range = smoothstep(0.0, 1.0, RADIUS / max(abs(p.z - sceneZ), 1e-4));
        occlusion += inFront * range;
    }

    float ao = 1.0 - occlusion / float(SAMPLES);
    ao = mix(ao, 1.0, smoothstep(FADE_START, FADE_END, distance));
    fragColour = vec4(vec3(ao), 1.0);
}
