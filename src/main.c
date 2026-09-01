/**
 * Firmware entry — bring up hardware, show splash, hand off to menu loop.
 *
 * No feature logic here; keep boot path readable.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_panel.h"
#include "rotary_encoder.h"
#include "menu_screens.h"
#include "menu_nav.h"

static const char *TAG = "dedsec";

void app_main(void)
{
    lcd_panel_init();
    rotary_encoder_init();

    ESP_LOGI(TAG, "Splash");
    menu_screens_draw_splash();
    vTaskDelay(pdMS_TO_TICKS(SPLASH_MS));
    menu_screens_splash_glitch_out();

    menu_nav_run();
}
