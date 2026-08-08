// Bloom, pass 4 of 4: adds the blurred glow (uScene) to the picture as it was before the bloom (uOriginal).
// uOriginal is the frame as the stage began, so the bloom passes should come first in their stage.

const float STRENGTH = 0.6;

void main()
{
    vec3 original = texture(uOriginal, vUV).rgb;
    vec3 glow = texture(uScene, vUV).rgb;
    fragColour = vec4(original + glow * STRENGTH * (1.0 - original), 1.0);     // screen-like add, no clipping
}
