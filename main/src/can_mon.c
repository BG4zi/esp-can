#include "can_mon.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

#include "waveshare_twai_port.h"

#ifndef TAG
#define TAG "can_mon"
#endif

static QueueHandle_t s_evt_q = NULL;

static uint32_t s_rx_cnt = 0;
static uint32_t s_tx_cnt = 0;
static uint32_t s_drop_cnt = 0;

esp_err_t can_mon_init(size_t queue_len)
{
    if (s_evt_q) return ESP_OK;

    s_evt_q = xQueueCreate(queue_len, sizeof(can_evt_t));
    if (!s_evt_q) {
        return ESP_ERR_NO_MEM;
    }

    s_rx_cnt = 0;
    s_tx_cnt = 0;
    s_drop_cnt = 0;

    return ESP_OK;
}

QueueHandle_t can_mon_get_queue(void)
{
    return s_evt_q;
}

void can_mon_push_evt(bool is_tx, const twai_message_t *m)
{
    if (!s_evt_q || !m) return;

    can_evt_t e = {
        .t_us  = esp_timer_get_time(),
        .is_tx = is_tx,
        .msg   = *m,
    };

    if (xQueueSend(s_evt_q, &e, 0) != pdTRUE) {
        s_drop_cnt++;
        return;
    }

    if (is_tx) s_tx_cnt++;
    else       s_rx_cnt++;
}

uint32_t can_mon_get_rx_cnt(void)   { return s_rx_cnt; }
uint32_t can_mon_get_tx_cnt(void)   { return s_tx_cnt; }
uint32_t can_mon_get_drop_cnt(void) { return s_drop_cnt; }

esp_err_t can_mon_send_frame(const twai_message_t *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    if (!waveshare_twai_is_started()) return ESP_ERR_INVALID_STATE;

    /* Copy because send_can_frame takes by value in your port */
    twai_message_t tmp = *m;

    esp_err_t err = send_can_frame(tmp);
    if (err == ESP_OK) {
        can_mon_push_evt(true, &tmp);
    }
    return err;
}

void can_mon_rx_task(void *arg)
{
    (void)arg;

    while (1) {
        twai_message_t msg;
        esp_err_t err = waveshare_twai_receive(&msg, pdMS_TO_TICKS(1000));

        if (err == ESP_OK) {
            can_mon_push_evt(false, &msg);
            continue;
        }

        if (err == ESP_ERR_TIMEOUT) {
            /* Normal idle case. */
            continue;
        }

        ESP_LOGW(TAG, "CAN RX error: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
