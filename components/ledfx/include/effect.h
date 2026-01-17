#pragma once
#include <stddef.h>
#include <stdint.h>
#include <apa102_controller.h>

typedef struct effect effect_t;

typedef esp_err_t (*effect_init_fn)(effect_t *e, size_t num_leds);
typedef void      (*effect_update_fn)(effect_t *e, pixel_t *pixels, size_t num_leds, float dt_s, control_values_t *cv);
typedef void      (*effect_deinit_fn)(effect_t *e);

struct effect {
    const char       *name;     
    effect_init_fn    init;     // may be NULL
    effect_update_fn  update;   // required
    effect_deinit_fn  deinit;   // may be NULL
    void             *ctx;      // effect state (owned by effect)
};


void reset_cooldown(float *timer, float duration);