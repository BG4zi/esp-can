#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UI config */
typedef struct {
    int  side_w_pct;      /* Right panel width percent */
    int  padding;         /* Screen padding */
    int  drain_per_tick;  /* How many events to drain per LVGL tick */
    int  tick_ms;         /* LVGL timer period */
} ui_canmon_cfg_t;

/* Build UI and start LVGL timer that drains events from queue. */
esp_err_t ui_canmon_start(const ui_canmon_cfg_t *cfg, QueueHandle_t evt_q);

#ifdef __cplusplus
}
#endif
