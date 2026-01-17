#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "apa102_driver.h"

typedef struct
{
    spi_transaction_t transaction;
    volatile bool in_use;
    bool ready;
    uint8_t *data;
    size_t capacity;
    size_t used_length;
} buffer_chunk_t;

typedef struct
{
    buffer_chunk_t *chunks;
    int chunk_count;
}apa102_write_buffer_t;

esp_err_t apa102_buffer_create(apa102_write_buffer_t *buffer, size_t chunk_size, size_t chunk_count, size_t num_leds);
void apa102_buffer_destroy(apa102_write_buffer_t *buffer);
esp_err_t apa102_buffer_write(apa102_write_buffer_t *buffer, const void *data, size_t data_count, size_t data_size);
void apa102_buffer_prepare_flush(apa102_write_buffer_t *buffer);
void apa102_buffer_reset_data(apa102_write_buffer_t *buffer);