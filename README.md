# zmk-keypeek-layer-notifier

ZMK feature module for sending [KeyPeek](https://github.com/srwi/keypeek) layer/key events over Raw HID.

## Usage

1. Add the KeyPeek module to your `zmk-config/config/west.yml`:

   ```yaml
   manifest:
     remotes:
       - name: zmkfirmware
         url-base: https://github.com/zmkfirmware
       - name: zzeneg # <-- required for Raw HID module
         url-base: https://github.com/zzeneg
       - name: srwi # <-- required for KeyPeek module
         url-base: https://github.com/srwi
     projects:
       - name: zmk
         remote: zmkfirmware
         revision: main
         import: app/west.yml
       - name: zmk-raw-hid # <-- Raw HID module
         remote: zzeneg
         revision: main
       - name: zmk-keypeek-layer-notifier # <-- KeyPeek module
         remote: srwi
         revision: master
   ```

2. Add the `raw_hid_adapter` as an additional shield to your build, e.g. in `build.yaml`:
   
   ```yaml
   include:
     - board: nice_nano_v2
       shield: <existing shields> raw_hid_adapter # <-- required for Raw HID support
       snippet: studio-rpc-usb-uart # <-- required for ZMK Studio support
   ```
   
   **Note:** If you are using a split keyboard, the change above is only required for the central half.

3. Enable ZMK Studio support in your `.conf` file:
   
   ```conf
   CONFIG_ZMK_STUDIO=y
   ```
   
   If your keyboard does not support ZMK Studio yet, adding support is described in the [ZMK documentation](https://zmk.dev/docs/features/studio#adding-zmk-studio-support-to-a-keyboard).

KeyPeek will read layout and keymap directly from the device for ZMK without requiring additional configuration.

> [!NOTE]
> If the keyboard has been paired via Bluetooth before enabling raw HID support, re-pairing may be necessary to allow the new communication channel.
