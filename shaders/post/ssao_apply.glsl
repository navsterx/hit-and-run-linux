// Ambient occlusion (SSAO), pass 2 of 2: smooths the occlusion from ssao.glsl (without blurring across edges),
// brings it up to the screen's size and darkens the picture with it.

const float STRENGTH = 1.0;             // 0 = no darkening, 1 = as computed, more = stronger
const float EDGE_SHARPNESS = 60.0;      // how strictly the smoothing stops at depth edges
const bool SHOW_AO = false;             // true: show only the occlusion (black and white), for tuning

// ssao.glsl's distance from the camera, as its logarithm
float LogDistance(vec4 texel)
{
    float v = floor(texel.g * 255.0 + 0.5) * 256.0 + floor(texel.b * 255.0 + 0.5);
    return v / 65535.0 * log(uFar / uNear);
}

void main()
{
    vec3 colour = texture(uOriginal, vUV).rgb;
    if(uFar <= 0.0)
    {
        fragColour = vec4(SHOW_AO ? vec3(1.0) : colour, 1.0);
        return;
    }

    // a 4x4 average of ssao.glsl's pixels (it may work at a smaller size than the screen), sliding smoothly: 5x5
    // pixels, the outer ones partly. Pixels at a different distance (across an edge: a character against a wall)
    // count next to nothing, so the occlusion does not bleed from one onto the other.
    vec2 position = vUV * vec2(textureSize(uScene, 0));
    ivec2 corner = ivec2(floor(position)) - 2;
    vec2 f = fract(position);
    ivec2 last = textureSize(uScene, 0) - 1;
    float centre = log(max(LinearDepth(vUV), uNear) / uNear);
    float sum = 0.0;
    float weights = 0.0;
    for(int y = 0; y < 5; y++)
    {
        float wy = y == 0 ? 1.0 - f.y : y == 4 ? f.y : 1.0;
        for(int x = 0; x < 5; x++)
        {
            float wx = x == 0 ? 1.0 - f.x : x == 4 ? f.x : 1.0;
            vec4 texel = texelFetch(uScene, clamp(corner + ivec2(x, y), ivec2(0), last), 0);
            float w = wx * wy * exp(-EDGE_SHARPNESS * abs(LogDistance(texel) - centre));
            sum += texel.r * w;
            weights += w;
        }
    }
    // nothing at this distance nearby (a thin pole, a wire): leave it undarkened
    float ao = weights > 1e-4 ? sum / weights : 1.0;
    ao = clamp(1.0 - (1.0 - ao) * STRENGTH, 0.0, 1.0);

    fragColour = vec4(SHOW_AO ? vec3(ao) : colour * ao, 1.0);
}
