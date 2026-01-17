#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include <stddef.h>
#include <apa102_config.h>
#include <control_values.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pixel_t;

typedef struct apa102_controller apa102_controller_t;
typedef struct apa102 apa102_t;

void init_gamma(float gamma);
esp_err_t apa102_controller_init(apa102_config_t *cfg, apa102_controller_t **out);
void apa102_controller_deinit(apa102_controller_t *con);
esp_err_t write_pixel(apa102_controller_t *c, size_t index, pixel_t pixel);
esp_err_t write_pixel_rgbb(apa102_controller_t *c, size_t index, uint8_t red, uint8_t green, uint8_t blue);
const pixel_t *apa102_controller_pixel(const apa102_controller_t *c, size_t index);
esp_err_t flush_data(apa102_controller_t *c);
esp_err_t transmit_data(apa102_controller_t *c);
bool check_previous_transmission(apa102_controller_t *c);
esp_err_t update_active_effect(apa102_controller_t *c, float dt_s);
esp_err_t controller_set_effect(apa102_controller_t *c, const char *name);
void get_control_values(apa102_controller_t *c, control_values_t **out);