#pragma once
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <apa102_controller.h>

pixel_t color_from_index_factor(pixel_t *colors, size_t color_count, float factor);
float fbm2_led01(float x, float y, uint32_t seed);
static inline float lerpf(float a, float b, float t)
{
    return a + t * (b - a);
}