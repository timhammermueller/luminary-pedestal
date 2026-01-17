#include "esp_err.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <effect.h>
#include <apa102_controller.h>

typedef struct
{
    float led_cooldown;
    float led_delay;
    int led_index;
    int color_index;
    pixel_t *colors;
    size_t color_count;

} individual_test_effect_state_t;

static esp_err_t individual_test_effect_init(effect_t *e, size_t num_leds)
{
    individual_test_effect_state_t *s = calloc(1, sizeof(*s));
    if (!s)
        return ESP_ERR_NO_MEM;

    s->led_index = 0;
    s->color_index = 0;
    s->led_delay = 0.4f;
    s->led_cooldown = 2.0f;

    static pixel_t palette[] = {
        {.r = 255, .g = 0, .b = 0},
        {.r = 0, .g = 255, .b = 0},
        {.r = 0, .g = 0, .b = 255},
    };

    s->colors = palette;
    s->color_count = sizeof(palette) / sizeof(palette[0]);

    e->ctx = s;
    return ESP_OK;
}

static void individual_test_effect_update(effect_t *e, pixel_t *pixels, size_t n, float dt_s, control_values_t *cv)
{
    individual_test_effect_state_t *s = (individual_test_effect_state_t *)e->ctx;

    float *cooldown = &s->led_cooldown;
    *cooldown -= dt_s;
    if (*cooldown <= 0)
    {
        reset_cooldown(cooldown, s->led_delay);
        s->led_index++;
        if (s->led_index >= n)
        {
            s->led_index = 0;

            s->color_index++;
            if (s->color_index >= s->color_count)
            {
                s->color_index = 0;
            }
        }
    }
    for (int i = 0; i < n; i++)
    {
        if (i == s->led_index)
        {
            pixels[i] = s->colors[s->color_index];
        }
        else
        {
            pixels[i] = (pixel_t){.r = 55, .g = 55, .b = 55};
        }
    }
}

static void individual_test_effect_deinit(effect_t *e)
{
    free(e->ctx);
    e->ctx = NULL;
}

effect_t g_individual_test_effect = {
    .name = "individual_test_effect",
    .init = individual_test_effect_init,
    .update = individual_test_effect_update,
    .deinit = individual_test_effect_deinit,
    .ctx = NULL,
};
