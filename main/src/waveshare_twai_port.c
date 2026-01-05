#include "waveshare_twai_port.h"

#include "esp_log.h"
#include "freertos/semphr.h"

/* 500 kbit/s timing */
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
/* Accept all frames */
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
/* No-ACK mode; change to TWAI_MODE_NORMAL if you want ACK on the bus */
static const twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NO_ACK);

static bool s_started = false;
static SemaphoreHandle_t s_tx_mtx = NULL;

esp_err_t waveshare_twai_init(void)
{
    if (s_started) return ESP_OK;

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(EXAMPLE_TAG, "Driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(EXAMPLE_TAG, "twai_start failed: %s", esp_err_to_name(err));
        (void)twai_driver_uninstall();
        return err;
    }

    /* Enable TX/RX + error alerts */
    uint32_t alerts =
        TWAI_ALERT_TX_IDLE |
        TWAI_ALERT_TX_SUCCESS |
        TWAI_ALERT_TX_FAILED |
        TWAI_ALERT_RX_DATA |
        TWAI_ALERT_RX_QUEUE_FULL |
        TWAI_ALERT_ERR_PASS |
        TWAI_ALERT_BUS_ERROR |
        TWAI_ALERT_BUS_OFF;

    (void)twai_reconfigure_alerts(alerts, NULL);

    s_tx_mtx = xSemaphoreCreateMutex();
    if (!s_tx_mtx) {
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    ESP_LOGI(EXAMPLE_TAG, "TWAI started (TX=%d, RX=%d, 500k)", TX_GPIO_NUM, RX_GPIO_NUM);
    return ESP_OK;
}

esp_err_t waveshare_twai_deinit(void)
{
    if (!s_started) return ESP_OK;

    if (s_tx_mtx) {
        vSemaphoreDelete(s_tx_mtx);
        s_tx_mtx = NULL;
    }

    (void)twai_stop();
    (void)twai_driver_uninstall();
    s_started = false;

    ESP_LOGI(EXAMPLE_TAG, "TWAI stopped");
    return ESP_OK;
}

bool waveshare_twai_is_started(void)
{
    return s_started;
}

esp_err_t send_can_frame(twai_message_t frame)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;

    if (frame.data_length_code > 8) frame.data_length_code = 8;

    /* Note:
     * For 29-bit IDs you must set frame.flags |= TWAI_MSG_FLAG_EXTD
     * For RTR you must set frame.rtr = 1 (or TWAI_MSG_FLAG_RTR depending on IDF version)
     */

    esp_err_t err;
    if (s_tx_mtx) xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    err = twai_transmit(&frame, pdMS_TO_TICKS(100));
    if (s_tx_mtx) xSemaphoreGive(s_tx_mtx);

    if (err != ESP_OK) {
        ESP_LOGW(EXAMPLE_TAG, "TX fail id=0x%08X ext=%d dlc=%u err=%s",
                 (unsigned)frame.identifier,
                 !!(frame.flags & TWAI_MSG_FLAG_EXTD),
                 (unsigned)frame.data_length_code,
                 esp_err_to_name(err));
    }

    return err;
}

esp_err_t waveshare_twai_receive(twai_message_t *out_frame, TickType_t timeout_ticks)
{
    if (!out_frame) return ESP_ERR_INVALID_ARG;
    if (!s_started) return ESP_ERR_INVALID_STATE;

    esp_err_t err = twai_receive(out_frame, timeout_ticks);
    if (err == ESP_OK) {
        /* out_frame now contains the received CAN frame */
        return ESP_OK;
    }

    if (err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(EXAMPLE_TAG, "RX fail err=%s", esp_err_to_name(err));
    }

    return err;
}

int waveshare_twai_drain(twai_message_t *out_frames, int max_frames)
{
    if (!out_frames || max_frames <= 0) return 0;
    if (!s_started) return 0;

    int n = 0;
    while (n < max_frames) {
        twai_message_t m;
        if (twai_receive(&m, 0) != ESP_OK) break;
        out_frames[n++] = m;
    }
    return n;
}
