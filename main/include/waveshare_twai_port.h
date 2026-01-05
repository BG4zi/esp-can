#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Pin configuration (use Kconfig if provided, otherwise fallback) ---- */
#ifndef TX_GPIO_NUM
#  ifdef CONFIG_EXAMPLE_TX_GPIO_NUM
#    define TX_GPIO_NUM CONFIG_EXAMPLE_TX_GPIO_NUM
#  else
#    define TX_GPIO_NUM 21
#  endif
#endif

#ifndef RX_GPIO_NUM
#  ifdef CONFIG_EXAMPLE_RX_GPIO_NUM
#    define RX_GPIO_NUM CONFIG_EXAMPLE_RX_GPIO_NUM
#  else
#    define RX_GPIO_NUM 22
#  endif
#endif

#ifndef EXAMPLE_TAG
#define EXAMPLE_TAG "TWAI Master"
#endif

esp_err_t waveshare_twai_init(void);
esp_err_t waveshare_twai_deinit(void);
bool      waveshare_twai_is_started(void);

esp_err_t send_can_frame(twai_message_t frame);

/* Receive one CAN frame (blocking up to timeout_ticks) */
esp_err_t waveshare_twai_receive(twai_message_t *out_frame, TickType_t timeout_ticks);

/* Optional: drain RX queue quickly (non-blocking) */
int waveshare_twai_drain(twai_message_t *out_frames, int max_frames);

#ifdef __cplusplus
}
#endif
