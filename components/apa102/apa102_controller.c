#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "apa102_driver.h"
#include "apa102_controller.h"
#include "driver/spi_master.h"
#include <apa102_config.h>
#include "esp_check.h"
#include "esp_err.h"
#include <effects_registry.h>
#include <effect.h>
#include "esp_log.h"
#include <math.h>

struct apa102_controller
{
    apa102_config_t *config;
    effect_t *active;
    pixel_t *pixels;
    size_t pixel_count;
    apa102_t *driver;

    apa102_data_frame_t *transfer_data_frames;
    size_t transfer_data_frame_capacity;    
    control_values_t control_values;
};

static uint8_t gamma8[256];
static const char *TAG = "apa102_controller";

void init_gamma(float gamma)
{
    for (int i = 0; i < 256; i++)
    {
        float x = (float)i / 255.0f;
        gamma8[i] = (uint8_t)lroundf(powf(x, gamma) * 255.0f);
    }
}
esp_err_t apa102_controller_init(apa102_config_t *cfg, apa102_controller_t **out)
{
    apa102_controller_t *con = calloc(1, sizeof(*con));
    ESP_RETURN_ON_FALSE(con, ESP_ERR_NO_MEM, TAG, "no mem");

    con->config = cfg;
    con->pixel_count = cfg->num_leds;
    con->pixels = calloc(cfg->num_leds, sizeof(pixel_t));

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(con->pixels, ESP_ERR_NO_MEM, fail, TAG, "no mem");

    con->transfer_data_frame_capacity = cfg->num_leds;
    con->transfer_data_frames = calloc(cfg->num_leds, sizeof(apa102_data_frame_t));
    ESP_GOTO_ON_FALSE(con->transfer_data_frames, ESP_ERR_NO_MEM, fail, TAG, "no mem");

    ESP_GOTO_ON_ERROR(apa102_driver_init(cfg, &con->driver), fail, TAG, "driver init failed");

    *out = con;
    return ret;

fail:
    apa102_controller_deinit(con);
    *out = NULL;
    return ret;
}
void apa102_controller_deinit(apa102_controller_t *con)
{
    if (!con)
    {
        return;
    }
    if (con->pixels)
    {
        free(con->pixels);
    }
    if (con->transfer_data_frames)
    {
        free(con->transfer_data_frames);
    }
    if (con->driver)
    {
        apa102_driver_deinit(con->driver);
    }
    if (con->config)
    {
        con->config = NULL;
    }
    free(con);
}

esp_err_t write_pixel(apa102_controller_t *c, size_t index, pixel_t pixel)
{
    if (!c)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= c->pixel_count)
    {
        return ESP_ERR_INVALID_ARG;
    }
    c->pixels[index] = pixel;

    return ESP_OK;
}

esp_err_t write_pixel_rgbb(apa102_controller_t *c, size_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (!c)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= c->pixel_count)
    {
        return ESP_ERR_INVALID_ARG;
    }
    pixel_t *pixel = &c->pixels[index];
    pixel->b = blue;
    pixel->g = green;
    pixel->r = red;

    return ESP_OK;
}

const pixel_t *apa102_controller_pixel(const apa102_controller_t *c, size_t index)
{
    if (!c)
        return NULL;
    if (index >= c->pixel_count)
        return NULL;
    return &c->pixels[index];
}

static inline uint8_t apa102_hdr(uint8_t br)
{
    return (uint8_t)(0b11100000 | br);
}

esp_err_t flush_data(apa102_controller_t *c)
{
    if (!c)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (apa102_is_busy(c->driver))
    {
        return ESP_ERR_INVALID_STATE;
    }
    int global_brightness = (int)(c->control_values.brightness_norm * 30.5f + 1.0f);
    if (global_brightness < 0)
        global_brightness = 0;
    if (global_brightness > 31)
        global_brightness = 31;

    size_t shorter_buffer_lenght = c->transfer_data_frame_capacity < c->pixel_count ? c->transfer_data_frame_capacity : c->pixel_count;
    for (int i = 0; i < shorter_buffer_lenght; i++)
    {
        apa102_data_frame_t *data_frame = &c->transfer_data_frames[i];
        pixel_t *pixel = &c->pixels[i];

        data_frame->blue = gamma8[pixel->b];
        data_frame->green = gamma8[pixel->g];
        data_frame->red = gamma8[pixel->r];
        data_frame->brightness = apa102_hdr(global_brightness);
    }

    apa102_write_data(c->driver, c->transfer_data_frames, shorter_buffer_lenght);
    return ESP_OK;
}

esp_err_t transmit_data(apa102_controller_t *c)
{
    if (!c)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return apa102_enqueue_transmission(c->driver);
}
bool check_previous_transmission(apa102_controller_t *c)
{
    if (!c)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return apa102_poll_transmission_finished(c->driver);
}

esp_err_t controller_set_effect(apa102_controller_t *c, const char *name)
{
    effect_t *next = effect_find_by_name(name);
    if (!next)
        return ESP_ERR_NOT_FOUND;

    if (c->active && c->active->deinit)
    {
        c->active->deinit(c->active);
    }
    c->active = next;

    if (c->active->init)
    {
        return c->active->init(c->active, c->pixel_count);
    }
    return ESP_OK;
}

void get_control_values(apa102_controller_t *c, control_values_t **out){
    *out = &c->control_values;
}

esp_err_t update_active_effect(apa102_controller_t *c, float dt_s)
{
    if (!c->active)
    {
        return ESP_ERR_INVALID_STATE;
    }
    c->active->update(c->active, c->pixels, c->pixel_count, dt_s, &c->control_values);
    return ESP_OK;
}