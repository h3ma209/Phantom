/**
 * KY-040-style rotary encoder → discrete UI events.
 *
 * ISR decodes quadrature; poll drains a FreeRTOS queue (one event per detent).
 * Click is active-low SW with debounce window.
 */
#pragma once

#include <stdint.h>

#define ROTENC_PIN_CLK 25  /* P25 */
#define ROTENC_PIN_DT  26  /* P26 */
#define ROTENC_PIN_SW  27  /* P27 */

typedef enum {
    ROTENC_NONE = 0,
    ROTENC_CW,     /* next */
    ROTENC_CCW,    /* prev */
    ROTENC_CLICK,
} rotenc_event_t;

void rotary_encoder_init(void);
rotenc_event_t rotary_encoder_poll(void);
