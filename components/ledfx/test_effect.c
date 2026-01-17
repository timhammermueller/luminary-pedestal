#include "esp_err.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <effect.h>
#include <apa102_controller.h>
#include <effect_math.h>

typedef struct
{
    int phase;
    float phase_delay_min;
    float phase_delay_max;
    float phase_change_cooldown;
} test_effect_state_t;

static esp_err_t test_effect_init(effect_t *e, size_t num_leds)
{
    test_effect_state_t *s = calloc(1, sizeof(*s));
    if (!s)
        return ESP_ERR_NO_MEM;
    s->phase_delay_min = 0.25f;
    s->phase_delay_max = 5;
    s->phase = 0;
    e->ctx = s;
    return ESP_OK;
}

static void test_effect_update(effect_t *e, pixel_t *pixels, size_t n, float dt_s, control_values_t *cv)
{
    test_effect_state_t *s = (test_effect_state_t *)e->ctx;

    float *cooldown = &s->phase_change_cooldown;
    *cooldown -= dt_s;
    if (*cooldown <= 0)
    {
        float delay = lerpf(s->phase_delay_max, s->phase_delay_min, cv->speed_norm);
        reset_cooldown(cooldown, delay);
        s->phase ++;
        if(s->phase >1){
            s->phase = 0;
        }

        for(int i = 0; i < n; i++){
            if((i + s->phase) % 2 == 0)
            {
                pixels[i] = (pixel_t){255, 0, 0 };
            }
            else
            {
                pixels[i] = (pixel_t){0, 255, 255 };
            }
        }
    }
}

static void test_effect_deinit(effect_t *e)
{
    free(e->ctx);
    e->ctx = NULL;
}

effect_t g_test_effect = {
    .name = "test_effect",
    .init = test_effect_init,
    .update = test_effect_update,
    .deinit = test_effect_deinit,
    .ctx = NULL,
};
