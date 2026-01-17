#include <stdint.h>
#include <math.h>
#include <apa102_controller.h>
#include <effect_math.h>

#include <stdint.h>
#include <math.h>

static inline float saturate(float t){ return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
static inline uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
// Smoothstep-ish curve (cubic). Good enough for LEDs.
static inline float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
// Cheap centered contrast (fast)
static inline float contrast01(float t, float c){
    t = (t - 0.5f) * c + 0.5f;
    return saturate(t);
}
static inline float h01(int x, int y, uint32_t seed) {
    // Use top 24 bits as mantissa-ish
    return (hash2(x, y, seed) >> 8) * (1.0f / 16777216.0f); // 2^24
}
float noise2_value01(float x, float y, uint32_t seed) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float tx = x - (float)x0;
    float ty = y - (float)y0;

    float u = smooth(tx);
    float v = smooth(ty);

    float a = h01(x0, y0, seed);
    float b = h01(x1, y0, seed);
    float c = h01(x0, y1, seed);
    float d = h01(x1, y1, seed);

    float ab = lerpf(a, b, u);
    float cd = lerpf(c, d, u);
    return lerpf(ab, cd, v);
}
// fBM using 0..1 noise, returns 0..1
float fbm2_01(float x, float y, uint32_t seed, int octaves,
             float lacunarity, float gain)
{
    float amp = 1.0f;
    float freq = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;

    for(int i = 0; i < octaves; i++){
        float n = noise2_value01(x * freq, y * freq, seed + (uint32_t)i * 1013u); // 0..1
        sum  += n * amp;
        norm += amp;

        freq *= lacunarity; // usually 2.0
        amp  *= gain;       // usually 0.5
    }
    return sum / norm; // normalized 0..1
}

// Convenience: fBM + “scale it up” for LEDs
float fbm2_led01(float x, float y, uint32_t seed)
{
    // Tune these
    int   oct   = 4;     // 3-5 is typical for LEDs
    float lac   = 2.0f;  // frequency multiplier per octave
    float g     = 0.5f;  // amplitude multiplier per octave

    float t = fbm2_01(x, y, seed, oct, lac, g);

    // Push toward 0/1:
    t = contrast01(t, 1.8f);      // 1.3 mild, 1.8 strong, 2.3 very punchy

    // Optional extra “pop” (still cheap):
    // t = t*t*(3.f - 2.f*t);     // smoothstep: mild extra contrast

    return t;
}
pixel_t color_from_index_factor(pixel_t *colors, size_t color_count, float factor)
{
    if (factor <= 0.f)
        return colors[0];
    if (factor >= 1.f)
        return colors[color_count - 1];

    float pos = factor * (float)(color_count - 1);
    size_t i0 = (size_t)pos;
    size_t i1 = i0 + 1;
    if (i1 >= color_count)
        i1 = color_count - 1;

    float t = pos - (float)i0;

    pixel_t a = colors[i0];
    pixel_t b = colors[i1];

    return (pixel_t){
        .r = (uint8_t)lroundf(a.r + (b.r - a.r) * t),
        .g = (uint8_t)lroundf(a.g + (b.g - a.g) * t),
        .b = (uint8_t)lroundf(a.b + (b.b - a.b) * t),
    };
}
