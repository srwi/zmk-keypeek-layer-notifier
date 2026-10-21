#include <zmk/endpoints_types.h>
#include <zmk/event_manager.h>
#include <zmk/studio/rpc.h>

#include <raw_hid/events.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define KEYPEEK_STUDIO_RPC_MARKER 0xF2
#define KEYPEEK_RPC_MAX_CHUNK (CONFIG_RAW_HID_REPORT_SIZE - 2)

static bool hid_rx_enabled = false;
static volatile bool use_hid_tx = false;
static uint8_t hid_report_buf[CONFIG_RAW_HID_REPORT_SIZE];

static void keypeek_usb_tx_notify(struct ring_buf *tx_buf, size_t added, bool msg_done,
                                  void *user_data);
static void keypeek_ble_tx_notify(struct ring_buf *tx_buf, size_t added, bool msg_done,
                                  void *user_data);

static const struct zmk_rpc_transport *find_fallback_transport(enum zmk_transport transport) {
    STRUCT_SECTION_FOREACH(zmk_rpc_transport, t) {
        if (t->transport == transport && t->tx_notify != keypeek_usb_tx_notify &&
            t->tx_notify != keypeek_ble_tx_notify) {
            return t;
        }
    }
    return NULL;
}

static void flush_hid_tx_buf(struct ring_buf *tx_buf, bool msg_done) {
    while (ring_buf_size_get(tx_buf) >= KEYPEEK_RPC_MAX_CHUNK ||
           (msg_done && ring_buf_size_get(tx_buf) > 0)) {
        memset(hid_report_buf, 0, sizeof(hid_report_buf));
        hid_report_buf[0] = KEYPEEK_STUDIO_RPC_MARKER;

        uint32_t len = ring_buf_get(tx_buf, &hid_report_buf[2], KEYPEEK_RPC_MAX_CHUNK);
        if (len == 0) {
            break;
        }
        hid_report_buf[1] = (uint8_t)len;

        raise_raw_hid_sent_event((struct raw_hid_sent_event){
            .data = hid_report_buf,
            .length = sizeof(hid_report_buf),
        });
    }
}

static void handle_tx_notify(enum zmk_transport transport, struct ring_buf *tx_buf, size_t added,
                             bool msg_done, void *user_data) {
    if (use_hid_tx) {
        flush_hid_tx_buf(tx_buf, msg_done);
        if (msg_done) {
            use_hid_tx = false;
        }
        return;
    }

    const struct zmk_rpc_transport *fallback = find_fallback_transport(transport);
    if (fallback && fallback->tx_notify) {
        fallback->tx_notify(tx_buf, added, msg_done, user_data);
    } else {
        ring_buf_reset(tx_buf);
    }
}

static void *handle_tx_user_data(enum zmk_transport transport) {
    if (use_hid_tx) {
        ring_buf_reset(zmk_rpc_get_tx_buf());
        return NULL;
    }

    const struct zmk_rpc_transport *fallback = find_fallback_transport(transport);
    if (fallback && fallback->tx_user_data) {
        return fallback->tx_user_data();
    }
    return NULL;
}

static int handle_rx_start(enum zmk_transport transport) {
    hid_rx_enabled = true;
    const struct zmk_rpc_transport *fallback = find_fallback_transport(transport);
    if (fallback && fallback->rx_start) {
        return fallback->rx_start();
    }
    return 0;
}

static int handle_rx_stop(enum zmk_transport transport) {
    hid_rx_enabled = false;
    use_hid_tx = false;
    const struct zmk_rpc_transport *fallback = find_fallback_transport(transport);
    if (fallback && fallback->rx_stop) {
        return fallback->rx_stop();
    }
    return 0;
}

static int keypeek_usb_rx_start(void) { return handle_rx_start(ZMK_TRANSPORT_USB); }
static int keypeek_usb_rx_stop(void) { return handle_rx_stop(ZMK_TRANSPORT_USB); }
static void *keypeek_usb_tx_user_data(void) { return handle_tx_user_data(ZMK_TRANSPORT_USB); }
static void keypeek_usb_tx_notify(struct ring_buf *tx_buf, size_t added, bool msg_done,
                                  void *user_data) {
    handle_tx_notify(ZMK_TRANSPORT_USB, tx_buf, added, msg_done, user_data);
}

static int keypeek_ble_rx_start(void) { return handle_rx_start(ZMK_TRANSPORT_BLE); }
static int keypeek_ble_rx_stop(void) { return handle_rx_stop(ZMK_TRANSPORT_BLE); }
static void *keypeek_ble_tx_user_data(void) { return handle_tx_user_data(ZMK_TRANSPORT_BLE); }
static void keypeek_ble_tx_notify(struct ring_buf *tx_buf, size_t added, bool msg_done,
                                  void *user_data) {
    handle_tx_notify(ZMK_TRANSPORT_BLE, tx_buf, added, msg_done, user_data);
}

/*
 * Prefixed with `aa_` so Zephyr's SORT_BY_NAME iterable section places these
 * ahead of ZMK's built-in `uart` and `gatt` transports, allowing Raw HID (0xF2)
 * RPC requests to be handled while transparently delegating regular UART/GATT
 * traffic to the built-in transports.
 */
ZMK_RPC_TRANSPORT(aa_keypeek_usb, ZMK_TRANSPORT_USB, keypeek_usb_rx_start, keypeek_usb_rx_stop,
                  keypeek_usb_tx_user_data, keypeek_usb_tx_notify);

ZMK_RPC_TRANSPORT(aa_keypeek_ble, ZMK_TRANSPORT_BLE, keypeek_ble_rx_start, keypeek_ble_rx_stop,
                  keypeek_ble_tx_user_data, keypeek_ble_tx_notify);

static int keypeek_raw_hid_received_listener(const zmk_event_t *eh) {
    if (!hid_rx_enabled) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event == NULL || event->length < 2) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const uint8_t *data = event->data;
    size_t length = event->length;
    if (length >= 3 && data[0] == 0x00 && data[1] == KEYPEEK_STUDIO_RPC_MARKER) {
        data++;
        length--;
    }

    if (data[0] != KEYPEEK_STUDIO_RPC_MARKER) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t payload_len = data[1];
    if (payload_len == 0 || (size_t)payload_len + 2 > length) {
        return ZMK_EV_EVENT_HANDLED;
    }

    use_hid_tx = true;

    struct ring_buf *rx_buf = zmk_rpc_get_rx_buf();
    uint32_t offset = 0;
    while (offset < payload_len) {
        uint32_t written = ring_buf_put(rx_buf, &data[2 + offset], payload_len - offset);
        if (written > 0) {
            offset += written;
            zmk_rpc_rx_notify();
        } else {
            zmk_rpc_rx_notify();
            k_yield();
            if (ring_buf_space_get(rx_buf) == 0) {
                LOG_WRN("Studio RX ring buffer full, dropped %u bytes", payload_len - offset);
                break;
            }
        }
    }

    return ZMK_EV_EVENT_HANDLED;
}

ZMK_LISTENER(keypeek_studio_hid_rpc, keypeek_raw_hid_received_listener);
ZMK_SUBSCRIPTION(keypeek_studio_hid_rpc, raw_hid_received_event);
