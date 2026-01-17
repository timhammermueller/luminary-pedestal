#include "driver/spi_master.h"
#include "freertos/portmacro.h"
#include <apa102_config.h>
#include "esp_check.h"
#include "esp_err.h"
#include <apa102_driver.h>
#include <apa102_buffer.h>
#include "esp_log.h"

static const char *TAG = "apa102_driver";

struct apa102
{
    spi_host_device_t host;
    spi_device_handle_t spi;
    bool bus_initialized;

    apa102_write_buffer_t buffer;

    uint16_t transactions_queried;
    uint16_t transactions_done;

    volatile bool in_flight;

    uint8_t *start_bytes;
    uint8_t *end_bytes;
    size_t start_len;
    size_t end_len;

    portMUX_TYPE mux;
};

_Static_assert(sizeof(apa102_data_frame_t) == 4, "apa102_data_frame_t must be 4 bytes");

esp_err_t apa102_driver_init(const apa102_config_t *cfg, apa102_t **out)
{
    apa102_t *d = calloc(1, sizeof(*d));
    ESP_RETURN_ON_FALSE(d, ESP_ERR_NO_MEM, TAG, "no mem");
    d->host = cfg->host;

    // SPI bus init
    spi_bus_config_t buscfg =
        {
            .mosi_io_num = cfg->data_gpio,
            .miso_io_num = -1, // APA102 write-only
            .sclk_io_num = cfg->clock_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = cfg->chunk_size,
        };
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(spi_bus_initialize(d->host, &buscfg, SPI_DMA_CH_AUTO), fail, TAG, "spi_bus_initialize");
    d->bus_initialized = true;

    spi_device_interface_config_t devcfg =
        {
            .clock_speed_hz = cfg->clock_hz,
            .mode = 0,                          // APA102: SPI mode 0
            .spics_io_num = -1,                 // often -1
            .queue_size = cfg->chunk_count + 2, // chunk count + 2 for start and end frame
        };
    ESP_GOTO_ON_ERROR(spi_bus_add_device(d->host, &devcfg, &d->spi), fail, TAG, "spi_bus_add_device");
    ESP_GOTO_ON_ERROR(apa102_buffer_create(&d->buffer, cfg->chunk_size, cfg->chunk_count, cfg->num_leds), fail, TAG, "apa102_buffer_create");

    d->start_len = 16;
    d->start_bytes = heap_caps_malloc(d->start_len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    d->end_len = 2 + cfg->num_leds;
    d->end_bytes = heap_caps_malloc(d->end_len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    memset(d->start_bytes, 0x00, d->start_len);
    memset(d->end_bytes, 0xFF, d->end_len);

    d->mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    d->in_flight = false;

    *out = d;
    return ret;

fail:
    apa102_driver_deinit(d);
    *out = NULL;
    return ret;
}
void apa102_driver_deinit(apa102_t *d)
{
    if (!d)
        return;

    if (d->spi)
    {
        spi_bus_remove_device(d->spi);
        d->spi = NULL;
    }
    if (d->bus_initialized)
    {
        spi_bus_free(d->host);
        d->bus_initialized = false;
    }
    if (d->start_bytes)
    {
        heap_caps_free(d->start_bytes);
    }
    if (d->end_bytes)
    {
        heap_caps_free(d->end_bytes);
    }
    apa102_buffer_destroy(&d->buffer);

    free(d);
}

static bool apa102_try_begin_write(apa102_t *d)
{
    bool ok = false;
    portENTER_CRITICAL(&d->mux);
    if (!d->in_flight)
    {
        d->in_flight = true;
        ok = true;
    }
    portEXIT_CRITICAL(&d->mux);
    return ok; // false = busy
}
static void apa102_end_write(apa102_t *d)
{
    portENTER_CRITICAL(&d->mux);
    d->in_flight = false;
    portEXIT_CRITICAL(&d->mux);
}
esp_err_t apa102_write_data(apa102_t *apa102, const apa102_data_frame_t *frames, size_t frames_length)
{

    esp_err_t result = apa102_buffer_write(&apa102->buffer, apa102->start_bytes, apa102->start_len, sizeof(uint8_t));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "apa102_write_data >> Error writing start frames to buffer!");
        return result;
    }
    result = apa102_buffer_write(&apa102->buffer, frames, frames_length, sizeof(apa102_data_frame_t));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "apa102_write_data >> Error data frames to buffer!");
        return result;
    }
    result = apa102_buffer_write(&apa102->buffer, apa102->end_bytes, apa102->end_len, sizeof(uint8_t));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "apa102_write_data >> Error writing end frames to buffer!");
        return result;
    }
    return ESP_OK;
}
bool apa102_is_busy(apa102_t *d)
{
    bool busy;
    portENTER_CRITICAL(&d->mux);
    busy = d->in_flight;
    portEXIT_CRITICAL(&d->mux);
    return busy;
}
static esp_err_t spi_send_async_queue(apa102_t *apa102, spi_transaction_t *transaction)
{
    esp_err_t result = spi_device_queue_trans(apa102->spi, transaction, portMAX_DELAY);
    if (result == ESP_OK)
    {
        apa102->transactions_queried++;
    }
    return result;
}
esp_err_t apa102_enqueue_transmission(apa102_t *apa102)
{
    if (!apa102_try_begin_write(apa102))
    {
        ESP_LOGI(TAG, "Can not enqueue! Device is busy");
        return ESP_ERR_INVALID_STATE; // Device is busy
    }
    apa102->transactions_queried = 0;
    apa102->transactions_done = 0;

    apa102_write_buffer_t *buffer = &apa102->buffer;

    apa102_buffer_prepare_flush(buffer);
    for (int i = 0; i < buffer->chunk_count; i++)
    {
        spi_transaction_t *transaction = &buffer->chunks[i].transaction;
        if (transaction->length <= 0)
        {
            continue;
        }
        ESP_RETURN_ON_ERROR(spi_send_async_queue(apa102, transaction), TAG, "SPI queue transmission failed!");
    }
    apa102_buffer_reset_data(buffer);
    return ESP_OK;
}

bool apa102_poll_transmission_finished(apa102_t *apa102)
{
    bool busy = apa102_is_busy(apa102);
    if (!busy)
    {
        return true;
    }
    spi_transaction_t *trans = NULL;
    while (spi_device_get_trans_result(apa102->spi, &trans, 0) == ESP_OK)
    {
        apa102->transactions_done++;
    }
    if (busy && apa102->transactions_queried == apa102->transactions_done)
    {
        apa102_end_write(apa102);
        busy = false;
    }
    if (busy)
    {
        ESP_LOGW(TAG, "is previous transmission finished?: %s!", !busy ? "true" : "false");
    }
    return !busy;
}