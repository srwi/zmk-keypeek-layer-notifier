# zmk-keypeek-layer-notifier

ZMK feature module for sending [KeyPeek](https://github.com/srwi/keypeek) layer/key events over Raw HID.

## Usage

1. Add this module to your `zmk-config/config/west.yml`:

  ```yaml
  manifest:
    remotes:
      - name: zmkfirmware
        url-base: https://github.com/zmkfirmware
      - name: zzeneg # <-- required for Raw HID module
        url-base: https://github.com/zzeneg
      - name: srwi # <-- required for this module
        url-base: https://github.com/srwi
    projects:
      - name: zmk
        remote: zmkfirmware
        revision: main
        import: app/west.yml
      - name: zmk-raw-hid # <-- Raw HID module
        remote: zzeneg
        revision: main
      - name: zmk-keypeek-layer-notifier # <-- this module
        remote: srwi
        revision: main
  ```

2. Add the `raw_hid_adapter` as an additional shield to your build, e.g. in `build.yaml`:

  ```yaml
  include:
    - board: nice_nano_v2
      shield: iskra raw_hid_adapter # <-- required for Raw HID support
      snippet: studio-rpc-usb-uart # <-- required for ZMK Studio support
  ```

3. Enable Raw HID and ZMK Studio support in your `.conf` file:

  ```conf
  CONFIG_RAW_HID=y
  CONFIG_ZMK_STUDIO=y
  ```

KeyPeek will then read layout and keymap directly from the device for ZMK without requiring additional configuration.

> [!NOTE]
> If the keyboard has been paired via Bluetooth before enabling raw HID support, re-pairing may be necessary to allow the new communication channel.