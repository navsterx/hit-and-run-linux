// Ambient occlusion (SSAO), pass 2 of 2: smooths the occlusion from ssao.glsl (without blurring across edges) and
// darkens the picture with it.

const float STRENGTH = 1.5;             // 0 = no darkening, 1 = as computed, more = stronger
const float BLUR_RADIUS = 2.0;          // smoothing, in pixels per step
const float EDGE_SHARPNESS = 60.0;      // how strictly the smoothing stops at depth edges
const bool SHOW_AO = false;             // true: show only the occlusion (black and white), for tuning

void main()
{
    vec2 pixel = BLUR_RADIUS / uResolution;
    float centreDepth = LinearDepth(vUV);
    float sum = 0.0;
    float weights = 0.0;
    for(int y = -2; y <= 2; y++)
    {
        for(int x = -2; x <= 2; x++)
        {
            vec2 uv = vUV + vec2(float(x), float(y)) * pixel;
            // samples at a different depth (across an edge: a character against a wall) count next to nothing, so
            // the occlusion does not bleed from one onto the other
            float w = exp(-EDGE_SHARPNESS * abs(LinearDepth(uv) - centreDepth) / max(centreDepth, 0.1));
            sum += texture(uScene, uv).r * w;
            weights += w;
        }
    }
    float ao = sum / weights;
    ao = clamp(1.0 - (1.0 - ao) * STRENGTH, 0.0, 1.0);

    if(SHOW_AO)
    {
        fragColour = vec4(vec3(ao), 1.0);
        return;
    }
    vec3 colour = texture(uOriginal, vUV).rgb;
    fragColour = vec4(colour * ao, 1.0);
}
