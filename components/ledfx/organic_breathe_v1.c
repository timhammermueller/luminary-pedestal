#include "esp_err.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <effect.h>
#include <apa102_controller.h>
#include <effect_math.h>
#include "esp_log.h"

typedef struct
{
    float speed_min;
    float speed_max;
    float scale;
    float cur_y;
    pixel_t *colors;
    size_t color_count;
} organic_breathe_v1_effect_state_t;

static esp_err_t organic_breathe_v1_effect_init(effect_t *e, size_t num_leds)
{
    organic_breathe_v1_effect_state_t *s = calloc(1, sizeof(*s));
    if (!s)
        return ESP_ERR_NO_MEM;
    s->scale = 0.4f;
    s->speed_min = 0.1f;
    s->speed_max = 3;
    s->cur_y = 0;

    static pixel_t palette[] = {
        {.r = 0, .g = 0, .b = 255},
        {.r = 128, .g = 0, .b = 255},
        {.r = 255, .g = 0, .b = 255},
        {.r = 255, .g = 0, .b = 128},
        {.r = 255, .g = 0, .b = 0},
    };

    s->colors = palette;
    s->color_count = sizeof(palette) / sizeof(palette[0]);

    e->ctx = s;
    return ESP_OK;
}

static void organic_breathe_v1_effect_update(effect_t *e, pixel_t *pixels, size_t num_leds, float dt_s, control_values_t *cv)
{
    organic_breathe_v1_effect_state_t *s = (organic_breathe_v1_effect_state_t *)e->ctx;

    float *cur_y = &s->cur_y;
    float speed = lerpf(s->speed_min, s->speed_max, cv->speed_norm);
    float scale = s->scale;

    size_t color_count = s->color_count;

    const uint32_t color_seed = 8953;
    const uint32_t brightness_seed = 75025365;

    *cur_y += speed * dt_s;

    for (int i = 0; i < num_leds; i++)
    {
        float x = i * scale;
        float col_factor = fbm2_led01(x, *cur_y, color_seed);

        // ESP_LOGE("breathe effect", "Factor: %f", col_factor);
        float brightness_factor = 0.4f + (0.6f * fbm2_led01(x, *cur_y, brightness_seed));

        pixel_t color = color_from_index_factor(s->colors, color_count, col_factor);
        color.r = (uint8_t)lroundf(color.r * brightness_factor);
        color.g = (uint8_t)lroundf(color.g * brightness_factor);
        color.b = (uint8_t)lroundf(color.b * brightness_factor);
        pixels[i] = color;
    }
}

static void organic_breathe_v1_effect_deinit(effect_t *e)
{
    free(e->ctx);
    e->ctx = NULL;
}

effect_t g_organic_breathe_v1_effect = {
    .name = "organic_breathe_v1",
    .init = organic_breathe_v1_effect_init,
    .update = organic_breathe_v1_effect_update,
    .deinit = organic_breathe_v1_effect_deinit,
    .ctx = NULL,
};
