#include "encoder.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static int s_last_clk;
static int s_sw_down;
static int64_t s_sw_down_us;
static int64_t s_last_rot_us;

void encoder_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << ENC_PIN_CLK) | (1ULL << ENC_PIN_DT) | (1ULL << ENC_PIN_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    s_last_clk = gpio_get_level(ENC_PIN_CLK);
    s_sw_down = 0;
    s_sw_down_us = 0;
    s_last_rot_us = 0;
}

enc_event_t encoder_poll(void)
{
    const int64_t now = esp_timer_get_time();

    /* Rotation: sample on CLK edge, direction from DT. */
    int clk = gpio_get_level(ENC_PIN_CLK);
    if (clk != s_last_clk) {
        s_last_clk = clk;
        if (clk == 0 && (now - s_last_rot_us) > 2000) { /* falling edge + debounce */
            s_last_rot_us = now;
            int dt = gpio_get_level(ENC_PIN_DT);
            return dt ? ENC_CCW : ENC_CW;
        }
    }

    /* Click: SW active low, emit on release after short debounce. */
    int sw = gpio_get_level(ENC_PIN_SW);
    if (!s_sw_down && sw == 0) {
        s_sw_down = 1;
        s_sw_down_us = now;
    } else if (s_sw_down && sw == 1) {
        s_sw_down = 0;
        if ((now - s_sw_down_us) > 30000 && (now - s_sw_down_us) < 2000000) {
            return ENC_CLICK;
        }
    }

    return ENC_NONE;
}
