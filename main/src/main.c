#include <stdio.h>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "waveshare_rgb_lcd_port.h"
#include "waveshare_twai_port.h"

#include "can_mon.h"
#include "ui_canmon.h"

#define TAG "main"

/* -------- Configuration knobs -------- */
#ifndef CAN_MON_RX_QUEUE_LEN
#define CAN_MON_RX_QUEUE_LEN  64
#endif

#ifndef CAN_RX_TASK_STACK
#define CAN_RX_TASK_STACK     4096
#endif

#ifndef CAN_RX_TASK_PRIO
#define CAN_RX_TASK_PRIO      10
#endif

void app_main(void)
{
    /* Initialize NVS (safe even if not used later) */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize LCD + touch + LVGL port */
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());

    /* Initialize CAN (TWAI) */
    err = waveshare_twai_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TWAI init failed: %s", esp_err_to_name(err));
        /* Continue booting UI anyway (you can show an error state later) */
    } else {
        ESP_LOGI(TAG, "TWAI initialized");
    }

    /* Initialize CAN monitor (queue + counters) */
    ESP_ERROR_CHECK(can_mon_init(CAN_MON_RX_QUEUE_LEN));

    /* Build UI under LVGL lock (LVGL APIs are not thread-safe) */
    if (lvgl_port_lock(-1)) {
        ui_canmon_cfg_t ui_cfg = {
            .side_w_pct     = 33,
            .padding        = 12,
            .drain_per_tick = 16,
            .tick_ms        = 50,
        };
        ESP_ERROR_CHECK(ui_canmon_start(&ui_cfg, can_mon_get_queue()));
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to lock LVGL; UI not created");
    }

    /* Start CAN RX task */
    xTaskCreatePinnedToCore(
        can_mon_rx_task,
        "can_rx_task",
        CAN_RX_TASK_STACK,
        NULL,
        CAN_RX_TASK_PRIO,
        NULL,
        0
    );

    /* Keep app_main alive */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
