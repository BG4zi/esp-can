#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event type delivered from CAN tasks to UI */
typedef struct {
    int64_t        t_us;   /* esp_timer_get_time() timestamp */
    bool           is_tx;  /* true: TX, false: RX */
    twai_message_t msg;    /* raw TWAI message */
} can_evt_t;

/* Initialize internal queue and counters */
esp_err_t can_mon_init(size_t queue_len);

/* Get queue handle for UI to drain events */
QueueHandle_t can_mon_get_queue(void);

/* Push an event (TX/RX) into queue (non-blocking). Safe to call from tasks. */
void can_mon_push_evt(bool is_tx, const twai_message_t *m);

/* Stats getters (atomic enough for UI reads) */
uint32_t can_mon_get_rx_cnt(void);
uint32_t can_mon_get_tx_cnt(void);
uint32_t can_mon_get_drop_cnt(void);

/* CAN RX task entry point */
void can_mon_rx_task(void *arg);

/* Convenience: transmit frame then push TX event if OK */
esp_err_t can_mon_send_frame(const twai_message_t *m);

#ifdef __cplusplus
}
#endif
