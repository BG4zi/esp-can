// main/src/ui_canmon.c
//
// Dark theme CAN monitor UI for LVGL:
// - Left: title + counters + scrolling log
// - Right: quick TX buttons (configurable table)
//
// This module does not start/stop CAN. It only renders events from a queue and
// calls can_mon_send_frame() when a button is pressed.

#include "ui_canmon.h"

#include <stdio.h>
#include <inttypes.h>

#include "esp_log.h"
#include "driver/twai.h"

#include "can_mon.h"

#ifndef TAG
#define TAG "ui_canmon"
#endif

/* ---------------- Theme knobs (dark) ---------------- */

#ifndef UI_BG_HEX
#define UI_BG_HEX        0x0B0F19  /* screen background */
#endif
#ifndef UI_PANEL_HEX
#define UI_PANEL_HEX     0x111827  /* card/panel background */
#endif
#ifndef UI_TEXT_HEX
#define UI_TEXT_HEX      0xE5E7EB  /* primary text */
#endif
#ifndef UI_MUTED_HEX
#define UI_MUTED_HEX     0x9CA3AF  /* secondary text */
#endif
#ifndef UI_BORDER_HEX
#define UI_BORDER_HEX    0x1F2937  /* borders / separators */
#endif
#ifndef UI_BTN_HEX
#define UI_BTN_HEX       0x1F2937  /* button background */
#endif
#ifndef UI_BTN_PR_HEX
#define UI_BTN_PR_HEX    0x374151  /* button pressed background */
#endif

#ifndef UI_RADIUS
#define UI_RADIUS        14
#endif
#ifndef UI_BORDER_W
#define UI_BORDER_W      2
#endif

/* ---------------- Button -> Frame mapping ---------------- */

typedef struct {
    const char    *label;
    twai_message_t frame;
} can_btn_cfg_t;

/* Helper macros to define frames easily */
#define CAN_FRAME_STD(_id, _dlc, b0,b1,b2,b3,b4,b5,b6,b7) \
    (twai_message_t){ \
        .identifier = (_id), \
        .data_length_code = (_dlc), \
        .flags = 0, \
        .data = { (uint8_t)(b0),(uint8_t)(b1),(uint8_t)(b2),(uint8_t)(b3), \
                  (uint8_t)(b4),(uint8_t)(b5),(uint8_t)(b6),(uint8_t)(b7) } \
    }

#define CAN_FRAME_EXT(_id, _dlc, b0,b1,b2,b3,b4,b5,b6,b7) \
    (twai_message_t){ \
        .identifier = (_id), \
        .data_length_code = (_dlc), \
        .flags = TWAI_MSG_FLAG_EXTD, \
        .data = { (uint8_t)(b0),(uint8_t)(b1),(uint8_t)(b2),(uint8_t)(b3), \
                  (uint8_t)(b4),(uint8_t)(b5),(uint8_t)(b6),(uint8_t)(b7) } \
    }

/* Edit this table to add/remove quick transmit buttons */
static const can_btn_cfg_t s_buttons[] = {
    {"Ping", CAN_FRAME_STD(0x123, 1, 0x01, 0, 0, 0, 0, 0, 0, 0)},
    {"PONG", CAN_FRAME_EXT(0x18FEA831, 1, 0x09, 0, 0, 0, 0, 0, 0, 0)},
    {"WABA", CAN_FRAME_EXT(0x18FEA839, 3, 0x02, 0x03, 0x04, 0, 0, 0, 0, 0)},
    {"LABA",
     CAN_FRAME_EXT(0x18FEA810, 6, 0x01, 0x00, 0x20, 0x80, 0x00, 0x00, 0, 0)},
    {"DUB",
     CAN_FRAME_EXT(0x18FEA811, 6, 0x02, 0x03, 0x04, 0x03, 0x00, 0x00, 0, 0)},
    { "DUB",    CAN_FRAME_EXT(0x18FEA811,6, 0x02,0x03,0x08,0x03,0x00,0x00,0,0) },
};

static const size_t s_button_count = sizeof(s_buttons) / sizeof(s_buttons[0]);

/* ---------------- UI state ---------------- */

static QueueHandle_t s_evt_q = NULL;

static lv_obj_t *s_lbl_title = NULL;
static lv_obj_t *s_lbl_stats = NULL;
static lv_obj_t *s_ta_log    = NULL;

static int s_drain_per_tick = 16;

/* A couple of reusable styles */
static lv_style_t s_st_scr;
static lv_style_t s_st_panel;
static lv_style_t s_st_title;
static lv_style_t s_st_muted;
static lv_style_t s_st_log;
static lv_style_t s_st_btn;
static lv_style_t s_st_btn_pr;
static bool s_styles_inited = false;

/* ---------------- Styles ---------------- */

static void init_styles_once(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_st_scr);
    lv_style_set_bg_color(&s_st_scr, lv_color_hex(UI_BG_HEX));
    lv_style_set_bg_opa(&s_st_scr, LV_OPA_COVER);
    lv_style_set_text_color(&s_st_scr, lv_color_hex(UI_TEXT_HEX));

    lv_style_init(&s_st_panel);
    lv_style_set_bg_color(&s_st_panel, lv_color_hex(UI_PANEL_HEX));
    lv_style_set_bg_opa(&s_st_panel, LV_OPA_COVER);
    lv_style_set_radius(&s_st_panel, UI_RADIUS);
    lv_style_set_border_width(&s_st_panel, UI_BORDER_W);
    lv_style_set_border_color(&s_st_panel, lv_color_hex(UI_BORDER_HEX));
    lv_style_set_pad_all(&s_st_panel, 12);
    lv_style_set_text_color(&s_st_panel, lv_color_hex(UI_TEXT_HEX));

    lv_style_init(&s_st_title);
    lv_style_set_text_color(&s_st_title, lv_color_hex(UI_TEXT_HEX));
    lv_style_set_text_font(&s_st_title, &lv_font_montserrat_26);

    lv_style_init(&s_st_muted);
    lv_style_set_text_color(&s_st_muted, lv_color_hex(UI_MUTED_HEX));
    lv_style_set_text_font(&s_st_muted, &lv_font_montserrat_16);

    lv_style_init(&s_st_log);
    lv_style_set_bg_color(&s_st_log, lv_color_hex(0x0A1220));
    lv_style_set_bg_opa(&s_st_log, LV_OPA_COVER);
    lv_style_set_radius(&s_st_log, 12);
    lv_style_set_border_width(&s_st_log, 1);
    lv_style_set_border_color(&s_st_log, lv_color_hex(UI_BORDER_HEX));
    lv_style_set_pad_all(&s_st_log, 10);
    lv_style_set_text_color(&s_st_log, lv_color_hex(0xD1D5DB)); /* slightly softer than UI_TEXT */

    lv_style_init(&s_st_btn);
    lv_style_set_bg_color(&s_st_btn, lv_color_hex(UI_BTN_HEX));
    lv_style_set_bg_opa(&s_st_btn, LV_OPA_COVER);
    lv_style_set_radius(&s_st_btn, 12);
    lv_style_set_border_width(&s_st_btn, 1);
    lv_style_set_border_color(&s_st_btn, lv_color_hex(UI_BORDER_HEX));
    lv_style_set_text_color(&s_st_btn, lv_color_hex(UI_TEXT_HEX));
    lv_style_set_pad_all(&s_st_btn, 10);

    lv_style_init(&s_st_btn_pr);
    lv_style_set_bg_color(&s_st_btn_pr, lv_color_hex(UI_BTN_PR_HEX));
    lv_style_set_bg_opa(&s_st_btn_pr, LV_OPA_COVER);
}

/* ---------------- Formatting helpers ---------------- */

/* Format one CAN event into a single short line suitable for a log view */
static void format_can_line(char *out, size_t out_sz, const can_evt_t *e)
{
    const twai_message_t *m = &e->msg;

    int p = 0;
    p += snprintf(out + p, out_sz - p, "%s ", e->is_tx ? "TX" : "RX");

    if (m->flags & TWAI_MSG_FLAG_EXTD) {
        p += snprintf(out + p, out_sz - p, "ID=%08" PRIX32 " ", (uint32_t)m->identifier);
    } else {
        p += snprintf(out + p, out_sz - p, "ID=%03" PRIX32 " ", (uint32_t)m->identifier);
    }

    p += snprintf(out + p, out_sz - p, "DLC=%u ", (unsigned)m->data_length_code);

    if (m->flags & TWAI_MSG_FLAG_RTR) {
        p += snprintf(out + p, out_sz - p, "RTR");
        return;
    }

    p += snprintf(out + p, out_sz - p, "DATA=");
    for (int i = 0; i < m->data_length_code && i < 8; i++) {
        p += snprintf(out + p, out_sz - p, "%02X%s",
                      (unsigned)m->data[i], (i == m->data_length_code - 1) ? "" : " ");
    }
}

/* Push line into LVGL text area and update the stats label */
static void ui_push_event(const can_evt_t *e)
{
    if (!s_ta_log || !s_lbl_stats) return;

    char line[180];
    format_can_line(line, sizeof(line), e);

    lv_textarea_add_text(s_ta_log, line);
    lv_textarea_add_text(s_ta_log, "\n");
    lv_textarea_set_cursor_pos(s_ta_log, LV_TEXTAREA_CURSOR_LAST);

    char stats[96];
    snprintf(stats, sizeof(stats),
             "RX: %" PRIu32 "   TX: %" PRIu32 "   DROP: %" PRIu32,
             can_mon_get_rx_cnt(),
             can_mon_get_tx_cnt(),
             can_mon_get_drop_cnt());

    lv_label_set_text(s_lbl_stats, stats);
}

/* LVGL timer callback: drain events from queue and render them */
static void ui_tick_cb(lv_timer_t *t)
{
    (void)t;

    if (!s_evt_q) return;

    for (int i = 0; i < s_drain_per_tick; i++) {
        can_evt_t e;
        if (xQueueReceive(s_evt_q, &e, 0) != pdTRUE) break;
        ui_push_event(&e);
    }
}

/* ---------------- Button callback ---------------- */

/* Send the configured frame, and log it through can_mon */
static void btn_send_cb(lv_event_t *e)
{
    const can_btn_cfg_t *cfg = (const can_btn_cfg_t *)lv_event_get_user_data(e);
    if (!cfg) return;

    esp_err_t err = can_mon_send_frame(&cfg->frame);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Button TX failed (%s): %s", cfg->label, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Button TX OK: %s", cfg->label);
}

/* ---------------- UI build ---------------- */

static lv_obj_t *panel_create(lv_obj_t *parent)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_add_style(p, &s_st_panel, 0);
    return p;
}

static void ui_build_split(const ui_canmon_cfg_t *cfg)
{
    init_styles_once();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &s_st_scr, 0);
    lv_obj_set_style_pad_all(scr, cfg->padding, 0);

    /* Root horizontal container (transparent) */
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);

    /* Left (log) panel */
    lv_obj_t *left = panel_create(row);
    lv_obj_set_size(left, lv_pct(100 - cfg->side_w_pct), lv_pct(100));

    s_lbl_title = lv_label_create(left);
    lv_obj_add_style(s_lbl_title, &s_st_title, 0);
    lv_label_set_text(s_lbl_title, "ESP-CAN Monitor");
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_stats = lv_label_create(left);
    lv_obj_add_style(s_lbl_stats, &s_st_muted, 0);
    lv_label_set_text(s_lbl_stats, "RX: 0   TX: 0   DROP: 0");
    lv_obj_align_to(s_lbl_stats, s_lbl_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_ta_log = lv_textarea_create(left);
    lv_obj_add_style(s_ta_log, &s_st_log, 0);
    lv_obj_set_width(s_ta_log, lv_pct(100));
    lv_obj_set_height(s_ta_log, lv_pct(82));
    lv_obj_align(s_ta_log, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_textarea_set_text(s_ta_log, "");
    lv_textarea_set_placeholder_text(s_ta_log, "CAN frames will appear here...");
    lv_textarea_set_cursor_click_pos(s_ta_log, false);
    lv_textarea_set_one_line(s_ta_log, false);
    lv_textarea_set_max_length(s_ta_log, 8192);

    /* Right (buttons) panel */
    lv_obj_t *right = panel_create(row);
    lv_obj_set_size(right, lv_pct(cfg->side_w_pct), lv_pct(100));

    lv_obj_t *rt = lv_label_create(right);
    lv_obj_add_style(rt, &s_st_muted, 0);
    lv_label_set_text(rt, "Quick TX");
    lv_obj_align(rt, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Buttons container inside right panel */
    lv_obj_t *grid = lv_obj_create(right);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_height(grid, lv_pct(92));
    lv_obj_align(grid, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);

    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (size_t i = 0; i < s_button_count; i++) {
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_add_style(b, &s_st_btn, 0);
        lv_obj_add_style(b, &s_st_btn_pr, LV_STATE_PRESSED);

        lv_obj_set_width(b, lv_pct(48));
        lv_obj_set_height(b, 62);

        lv_obj_add_event_cb(b, btn_send_cb, LV_EVENT_CLICKED, (void *)&s_buttons[i]);

        lv_obj_t *lbl = lv_label_create(b);
        lv_label_set_text(lbl, s_buttons[i].label);
        lv_obj_center(lbl);
    }
}

esp_err_t ui_canmon_start(const ui_canmon_cfg_t *cfg_in, QueueHandle_t evt_q)
{
    if (!evt_q) return ESP_ERR_INVALID_ARG;

    ui_canmon_cfg_t cfg = {
        .side_w_pct     = 33,
        .padding        = 12,
        .drain_per_tick = 16,
        .tick_ms        = 50,
    };

    if (cfg_in) cfg = *cfg_in;

    s_evt_q = evt_q;
    s_drain_per_tick = cfg.drain_per_tick;

    ui_build_split(&cfg);

    /* Timer that pulls CAN events and paints them */
    lv_timer_create(ui_tick_cb, cfg.tick_ms, NULL);

    return ESP_OK;
}
