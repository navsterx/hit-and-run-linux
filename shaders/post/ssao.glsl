// Ambient occlusion (SSAO), pass 1 of 2: how hidden each point is by the geometry around it (corners, the ground
// under cars and around feet, where walls meet the ground), from the depth buffer. Outputs the occlusion in red and
// the distance from the camera in green and blue; ssao_apply.glsl smooths the occlusion and darkens the picture
// with it. Put both in [scene], in that order.

#pragma shar_original                  // the effect starts here: ssao_apply reads this pass's input as uOriginal
#pragma shar_scale 0.5                  // works at half the screen's width and height (a quarter of the cost);
                                        // 1.0 = full size: a little sharper, four times slower

const int SAMPLES = 8;                  // more = smoother, slower (16: hardly smoother, a third slower)
const float RADIUS = 0.8;               // how far around a point to look (game units, about metres)
const float BIAS = 0.04;                // ignore tiny depth differences (stops flat surfaces darkening themselves)
const float FADE_START = 40.0;          // occlusion fades out between these distances from the camera ...
const float FADE_END = 90.0;            // ... (far away the depth buffer is too coarse)

// a different rotation of the sample pattern for each pixel of a 4x4 block: ssao_apply's smoothing covers 4x4
// pixels, so it averages all 16 rotations
const int ROTATION[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);

// the distance, for ssao_apply: logarithmic (equally precise near and far), 16 bits over green and blue
vec4 Output(float ao, float distance)
{
    float t = clamp(log(max(distance, uNear) / uNear) / log(uFar / uNear), 0.0, 1.0);
    float v = floor(t * 65535.0 + 0.5);
    return vec4(ao, floor(v / 256.0) / 255.0, mod(v, 256.0) / 255.0, 1.0);
}

void main()
{
    float depth = texture(uDepth, vUV).r;
    if(uFar <= 0.0 || depth >= 1.0)
    {
        fragColour = vec4(1.0);         // no 3D view, or the sky (as far as it gets)
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

    ivec2 cell = ivec2(gl_FragCoord.xy) & 3;
    float noise = (float(ROTATION[cell.y * 4 + cell.x]) + 0.5) / 16.0;
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
        vec3 q = ViewPosition(uv);

        // occluded when the surface there is in front of the sample point, above this surface (not just in front
        // of it on screen) and within RADIUS of this point in 3D: a character standing in front of a wall is far
        // from the wall in 3D, so it leaves no dark halo on it
        vec3 toOccluder = q - p;
        float distance3D = length(toOccluder);
        float inFront = q.z >= s.z + BIAS ? 1.0 : 0.0;
        float above = dot(toOccluder, n) > BIAS * distance3D ? 1.0 : 0.0;
        float range = 1.0 - smoothstep(0.5 * RADIUS, RADIUS, distance3D);
        occlusion += inFront * above * range;
    }

    float ao = 1.0 - occlusion / float(SAMPLES);
    ao = mix(ao, 1.0, smoothstep(FADE_START, FADE_END, distance));
    fragColour = Output(ao, distance);
}
