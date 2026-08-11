#pragma once

#include <stdint.h>

/* KY-040 style: CLK/DT rotate, SW click (active low). */
#define ENC_PIN_CLK 25  /* P25 */
#define ENC_PIN_DT  26  /* P26 */
#define ENC_PIN_SW  27  /* P27 */

typedef enum {
    ENC_NONE = 0,
    ENC_CW,     /* clockwise → next */
    ENC_CCW,    /* counter-clockwise → prev */
    ENC_CLICK,  /* SW press */
} enc_event_t;

void encoder_init(void);
enc_event_t encoder_poll(void);
