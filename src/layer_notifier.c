#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#include <raw_hid/events.h>

#include <string.h>
#include <stdint.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define KEYPEEK_LAYER_PACKET_MARKER 0xFF
#define KEYPEEK_KEY_PACKET_MARKER 0xF1
#define MAX_LAYER_CHECK 32

static uint8_t hid_buf[CONFIG_RAW_HID_REPORT_SIZE];

static void keypeek_send_layer_state(void) {
    uint32_t layer_state = 0;
    for (uint8_t i = 0; i < MAX_LAYER_CHECK; i++) {
        if (zmk_keymap_layer_active(i)) {
            layer_state |= BIT(i);
        }
    }

    uint32_t default_layer_state = BIT(0);

    memset(hid_buf, 0, sizeof(hid_buf));
    hid_buf[0] = KEYPEEK_LAYER_PACKET_MARKER;
    hid_buf[1] = sizeof(uint32_t);
    memcpy(&hid_buf[2], &default_layer_state, sizeof(uint32_t));
    memcpy(&hid_buf[2 + sizeof(uint32_t)], &layer_state, sizeof(uint32_t));

    raise_raw_hid_sent_event((struct raw_hid_sent_event){.data = hid_buf, .length = sizeof(hid_buf)});
}

static void keypeek_send_key_event(uint32_t position, bool pressed) {
    if (position > UINT8_MAX) {
        LOG_WRN("Position %u exceeds KeyPeek packet format", position);
        return;
    }

    memset(hid_buf, 0, sizeof(hid_buf));
    hid_buf[0] = KEYPEEK_KEY_PACKET_MARKER;
    hid_buf[1] = 0;
    hid_buf[2] = (uint8_t)position;
    hid_buf[3] = pressed ? 1 : 0;

    raise_raw_hid_sent_event((struct raw_hid_sent_event){.data = hid_buf, .length = sizeof(hid_buf)});
}

static int layer_state_changed_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    keypeek_send_layer_state();
    return ZMK_EV_EVENT_BUBBLE;
}

static int position_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    keypeek_send_key_event(ev->position, ev->state);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(keypeek_layer_notifier, layer_state_changed_listener);
ZMK_SUBSCRIPTION(keypeek_layer_notifier, zmk_layer_state_changed);

ZMK_LISTENER(keypeek_key_notifier, position_state_changed_listener);
ZMK_SUBSCRIPTION(keypeek_key_notifier, zmk_position_state_changed);
