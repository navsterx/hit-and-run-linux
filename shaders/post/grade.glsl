// Colour grading: exposure, contrast, saturation, warmth.

const float EXPOSURE = 1.0;             // brightness multiplier
const float CONTRAST = 1.05;            // 1 = unchanged
const float SATURATION = 1.15;          // 1 = unchanged, 0 = black and white
const float WARMTH = 0.02;              // > 0 warmer (more red/yellow), < 0 cooler

void main()
{
    vec3 c = texture(uScene, vUV).rgb * EXPOSURE;
    c = (c - 0.5) * CONTRAST + 0.5;
    float grey = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(grey), c, SATURATION);
    c += vec3(WARMTH, WARMTH * 0.3, -WARMTH);
    fragColour = vec4(clamp(c, 0.0, 1.0), 1.0);
}
