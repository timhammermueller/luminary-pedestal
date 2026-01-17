#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include <stddef.h>
#include <apa102_config.h>

typedef struct apa102 apa102_t; 

typedef struct __attribute__((packed)){
    uint8_t brightness; // 0b111xxxxx (5 bit for brightness)
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} apa102_data_frame_t;

esp_err_t apa102_driver_init(const apa102_config_t *cfg, apa102_t **out);
void apa102_driver_deinit(apa102_t *d);

esp_err_t apa102_enqueue_transmission(apa102_t *apa102);
bool apa102_poll_transmission_finished(apa102_t *apa102);
esp_err_t apa102_write_data(apa102_t *apa102, const apa102_data_frame_t *frames, size_t frames_length);
bool apa102_is_busy(apa102_t *d);