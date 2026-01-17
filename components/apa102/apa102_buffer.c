#include <string.h>
#include <stdbool.h>
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include <apa102_buffer.h>

static const char *TAG = "apa102_buffer";

esp_err_t apa102_buffer_create(apa102_write_buffer_t *buffer, size_t chunk_size, size_t chunk_count, size_t num_leds)
{
    if (chunk_size % 4 != 0)
    {
        ESP_LOGE(TAG, "The apa102 buffer chunk_size must be a multiple of 4 for pixel alignment!");
        return ESP_FAIL;
    }
    buffer->chunks = calloc(chunk_count, sizeof(buffer_chunk_t));
    ESP_RETURN_ON_FALSE(buffer->chunks, ESP_ERR_NO_MEM, TAG, "no memory");
    buffer->chunk_count = chunk_count;

    esp_err_t ret = ESP_OK;
    for (int i = 0; i < chunk_count; i++)
    {
        buffer_chunk_t *chunk = &(buffer->chunks[i]);
        chunk->data = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        ESP_GOTO_ON_FALSE(chunk->data, ESP_ERR_NO_MEM, fail, TAG, "no memory");

        chunk->capacity = chunk_size;
        chunk->used_length = 0;
    }

    return ret;

fail:
    apa102_buffer_destroy(buffer);
    return ret;
}

void apa102_buffer_destroy(apa102_write_buffer_t *buffer)
{
    if (!buffer)
        return;

    if (buffer->chunks)
    {
        for (int i = 0; i < buffer->chunk_count; i++)
        {
            buffer_chunk_t *chunk = &buffer->chunks[i];
            if (chunk->data)
            {
                heap_caps_free(chunk->data);
                chunk->data = NULL;
            }
        }
        free(buffer->chunks);
        buffer->chunks = NULL;
    }
    free(buffer);
}

static void append_data(uint8_t *dst, size_t dst_capacity, size_t *dst_free_index,
                   const void *data, size_t data_size,
                   size_t data_start, size_t data_count)
{
    size_t write_byte_count = data_count * data_size;

    if (*dst_free_index + write_byte_count > dst_capacity)
        return;

    memcpy(dst + *dst_free_index, data + data_start, write_byte_count);
    *dst_free_index += write_byte_count;
}
static esp_err_t get_chunk_with_space(apa102_write_buffer_t *buffer, buffer_chunk_t **out)
{
    buffer_chunk_t *chunk = NULL;
    for (int i = 0; i < buffer->chunk_count; i++)
    {
        buffer_chunk_t *possible_chunk = &buffer->chunks[i];
        if (possible_chunk->used_length < possible_chunk->capacity)
        {
            chunk = possible_chunk;
            chunk->in_use = true;
            break;
        }
    }
    if (!chunk)
    {
        ESP_LOGE(TAG, "All buffer chunks are full!");
        return ESP_FAIL;
    }
    *out = chunk;
    return ESP_OK;
}   
esp_err_t apa102_buffer_write(apa102_write_buffer_t *buffer, const void *data, size_t data_count, size_t data_size)
{
    if (!data || data_count < 1)
    {
        ESP_LOGW(TAG, "apa102_buffer_write>> no data to write!");
        return ESP_OK;
    }
    uint32_t data_written = 0;
    buffer_chunk_t *chunk;
    while (data_written < data_count)
    {
        if (get_chunk_with_space(buffer, &chunk) != ESP_OK)
        {
            ESP_LOGE(TAG, "apa102_buffer_write >> Could not find chunk with space!");
            return ESP_FAIL;
        }
        size_t free_bytes = chunk->capacity - chunk->used_length;
        size_t free_data_units = free_bytes / data_size;

        uint32_t remaining_data = data_count - data_written;
        uint32_t data_to_write = free_data_units < remaining_data ? free_data_units : remaining_data;

        append_data(chunk->data, chunk->capacity, &chunk->used_length, data, data_size, data_written, data_to_write);
        data_written += data_to_write;
    }
    return ESP_OK;
}

void apa102_buffer_prepare_flush(apa102_write_buffer_t *buffer)
{      
    for (int i = 0; i < buffer->chunk_count; i++)
    {
        buffer_chunk_t *chunk = &buffer->chunks[i];
        spi_transaction_t *transaction = &chunk->transaction;

        if (chunk->used_length == 0) {
            continue;
        }
        memset(transaction, 0, sizeof(spi_transaction_t)); //clear old data

        transaction->length = chunk->used_length * 8;
        transaction->tx_buffer = chunk->data;

        // ESP_LOGI(TAG, "transaction->length %u!",(chunk->used_length));
        // ESP_LOG_BUFFER_HEX(TAG, transaction->tx_buffer,chunk->used_length);
        chunk->ready = true;
    }
}

void apa102_buffer_reset_data(apa102_write_buffer_t *buffer)
{
    for (int i = 0; i < buffer->chunk_count; i++)
    {
        buffer_chunk_t *chunk = &buffer->chunks[i];

        chunk->ready = false;
        chunk->in_use = false;
        chunk->used_length = 0;
    }
}

