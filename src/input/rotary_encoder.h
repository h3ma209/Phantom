#pragma once

#include <stdint.h>

/* KY-040 style: CLK/DT rotate, SW click (active low). */
#define ROTENC_PIN_CLK 25  /* P25 */
#define ROTENC_PIN_DT  26  /* P26 */
#define ROTENC_PIN_SW  27  /* P27 */

typedef enum {
    ROTENC_NONE = 0,
    ROTENC_CW,     /* clockwise → next */
    ROTENC_CCW,    /* counter-clockwise → prev */
    ROTENC_CLICK,  /* SW press */
} rotenc_event_t;

void rotary_encoder_init(void);
rotenc_event_t rotary_encoder_poll(void);
