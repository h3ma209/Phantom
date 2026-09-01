/**
 * Encoder driver: GPIO ISR → Gray-code accumulate → detent events on queue.
 *
 * Poll from the UI task only. Do not call gpio from that path for CLK/DT;
 * ISR already owns the edges.
 */
#include "rotary_encoder.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

#define ROTENC_DETENT_STEPS 4
#define ROTENC_QUEUE_LEN    16
#define ROTENC_CLICK_MIN_US 25000
#define ROTENC_CLICK_MAX_US 1500000
#define ROTENC_ROT_COOLDOWN_US 8000

static QueueHandle_t s_q;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_prev;
static int8_t s_accum;
static int64_t s_last_emit_us;

static int s_sw_down;
static int64_t s_sw_down_us;

/* index = (prev<<2)|curr → +1/−1 step; illegal combos are 0 */
static const int8_t ROTENC_TRANS[16] = {
    0,  +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0,
};

static void IRAM_ATTR enc_isr(void *arg)
{
    (void)arg;
    const int clk = gpio_get_level(ROTENC_PIN_CLK);
    const int dt = gpio_get_level(ROTENC_PIN_DT);
    const uint8_t curr = (uint8_t)((clk << 1) | dt);
    const int64_t now = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&s_mux);
    const int8_t delta = ROTENC_TRANS[(s_prev << 2) | curr];
    s_prev = curr;
    if (delta == 0) {
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }

    s_accum = (int8_t)(s_accum + delta);
    rotenc_event_t ev = ROTENC_NONE;
    if (s_accum >= ROTENC_DETENT_STEPS) {
        s_accum = 0;
        if ((now - s_last_emit_us) >= ROTENC_ROT_COOLDOWN_US) {
            ev = ROTENC_CW;
            s_last_emit_us = now;
        }
    } else if (s_accum <= -ROTENC_DETENT_STEPS) {
        s_accum = 0;
        if ((now - s_last_emit_us) >= ROTENC_ROT_COOLDOWN_US) {
            ev = ROTENC_CCW;
            s_last_emit_us = now;
        }
    }
    portEXIT_CRITICAL_ISR(&s_mux);

    if (ev != ROTENC_NONE && s_q) {
        BaseType_t hpw = pdFALSE;
        xQueueSendFromISR(s_q, &ev, &hpw);
        if (hpw) {
            portYIELD_FROM_ISR();
        }
    }
}

void rotary_encoder_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << ROTENC_PIN_CLK) | (1ULL << ROTENC_PIN_DT) | (1ULL << ROTENC_PIN_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    s_q = xQueueCreate(ROTENC_QUEUE_LEN, sizeof(rotenc_event_t));
    s_prev = (uint8_t)((gpio_get_level(ROTENC_PIN_CLK) << 1) | gpio_get_level(ROTENC_PIN_DT));
    s_accum = 0;
    s_last_emit_us = 0;
    s_sw_down = 0;
    s_sw_down_us = 0;

    esp_err_t isr = gpio_install_isr_service(0);
    if (isr != ESP_OK && isr != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(isr);
    }
    ESP_ERROR_CHECK(gpio_set_intr_type(ROTENC_PIN_CLK, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_set_intr_type(ROTENC_PIN_DT, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ROTENC_PIN_CLK, enc_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ROTENC_PIN_DT, enc_isr, NULL));
}

rotenc_event_t rotary_encoder_poll(void)
{
    rotenc_event_t ev = ROTENC_NONE;
    if (s_q && xQueueReceive(s_q, &ev, 0) == pdTRUE) {
        return ev;
    }

    const int64_t now = esp_timer_get_time();
    const int sw = gpio_get_level(ROTENC_PIN_SW);
    if (!s_sw_down && sw == 0) {
        s_sw_down = 1;
        s_sw_down_us = now;
    } else if (s_sw_down && sw == 1) {
        s_sw_down = 0;
        if ((now - s_sw_down_us) > ROTENC_CLICK_MIN_US && (now - s_sw_down_us) < ROTENC_CLICK_MAX_US) {
            return ROTENC_CLICK;
        }
    }

    return ROTENC_NONE;
}
