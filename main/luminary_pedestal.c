#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <apa102_controller.h>
#include "esp_err.h"
#include <apa102_controller.h>
#include <apa102_config.h>
#include "esp_log.h"
#include <string.h>
#include "esp_adc/adc_oneshot.h"
#include <control_values.h>

static const char *TAG = "MAIN";

#define POT1_CH ADC_CHANNEL_8   // ADC1_CH8
#define POT2_CH ADC_CHANNEL_9   // ADC1_CH9

static adc_oneshot_unit_handle_t s_adc1 = NULL;

esp_err_t configure(apa102_config_t *config, apa102_controller_t **out)
{
    init_gamma(2);

    esp_err_t err = apa102_controller_init(config, out);
    if (err != ESP_OK)
    {
        return err;
    }
    // err = controller_set_effect(*out, "test_effect");
    err = controller_set_effect(*out, "organic_breathe_v1");
    // err = controller_set_effect(*out, "individual_test_effect");

    if (err != ESP_OK)
    {
        return err;
    }
    return err;
}
void setup_potentiometer_adc()
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc1 ));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // typically 12-bit -> 0..4095
        .atten    = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, POT1_CH, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, POT2_CH, &chan_cfg));
}
static int read_adc_stable(adc_channel_t ch)
{
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1, ch, &raw)); // throwaway
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1, ch, &raw)); // real
    return raw;
}
static int read_adc_avg_stable(adc_channel_t ch, int samples)
{
    int sum = 0;
    for (int i = 0; i < samples; i++) sum += read_adc_stable(ch);
    return sum / samples;
}

static int read_adc_avg(adc_oneshot_unit_handle_t adc, adc_channel_t ch, int samples)
{
    int sum = 0;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, ch, &raw));
        sum += raw;
    }
    return sum / samples;
}

static inline int raw_to_percent(int raw)
{
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (raw * 100) / 4095;
}

static inline float raw_to_norm(int raw)
{
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return ( 4095.0f - raw) / 4095.0f;
}

float get_adc_value_norm(adc_channel_t channel)
{
    int raw = read_adc_avg( s_adc1,channel, 16);
    float n = raw_to_norm(raw);

    // DEBUG:
    printf("ADC ch=%d raw=%d norm=%.4f\n", channel, raw, n);
    
    return n;
}

void update_control_values(control_values_t *cv){
    cv->brightness_norm = get_adc_value_norm(POT1_CH);
    cv->speed_norm = get_adc_value_norm(POT2_CH);
}

void main_loop(apa102_controller_t *controller)
{
    int frame_target_ms = 20;
    int64_t last_effect_us = esp_timer_get_time();

    TickType_t last_wake = xTaskGetTickCount();

    while (1)
    {
        int64_t now = esp_timer_get_time();
        int64_t dt_us = now - last_effect_us;
        last_effect_us = now;

        if (dt_us > frame_target_ms * 1000)
        {
            ESP_LOGI(TAG, "Can't keep up! This ticks DeltaTime: %lld, Target: %d", dt_us, (frame_target_ms * 1000));
        }
        control_values_t *control_values;
        get_control_values(controller,&control_values);
        update_control_values(control_values);

        float dt_s = dt_us / 1e6f;
        update_active_effect(controller, dt_s);

        if (check_previous_transmission(controller))
        {
            esp_err_t res = flush_data(controller);
            if (res != ESP_OK)
            {
                ESP_LOGI(TAG, "flush_data result: %s", esp_err_to_name(res));
            }

            transmit_data(controller);
        }
        else
        {
            ESP_LOGW(TAG, "Previous transmission is still ongoing!");
        }

        TickType_t period = pdMS_TO_TICKS(frame_target_ms);
        if (period == 0)
            period = 1;
        vTaskDelayUntil(&last_wake, period);
    }
}

void app_main(void)
{
    apa102_config_t config = (apa102_config_t){.clock_hz = 4 * 1000 * 1000,
                                               .num_leds = 47,
                                               .clock_gpio = 5,
                                               .data_gpio = 6,
                                               .chunk_size = 128,
                                               .chunk_count = 5,
                                               .host = SPI3_HOST
                                            };

    setup_potentiometer_adc();
    apa102_controller_t *controller;
    configure(&config, &controller);

    // controller_set_effect(con, "test_effect");

    main_loop(controller);
}
