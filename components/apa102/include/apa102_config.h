#pragma once
#include <stdint.h>
#include <stddef.h>
#include "driver/spi_master.h"
#include <control_values.h>

typedef struct{
    uint32_t clock_hz;
    uint16_t num_leds;
    int clock_gpio;
    int data_gpio;
    size_t chunk_size;
    size_t chunk_count;
    spi_host_device_t host;
} apa102_config_t;