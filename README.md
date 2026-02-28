# zmk-keypeek-layer-notifier

ZMK feature module for sending KeyPeek layer/key events over Raw HID.

## Module Metadata

- Module name: `zmk-keypeek-layer-notifier`
- Type: `feature`

## Usage

1. Add this repo to your `config/west.yml` projects list.
2. Build with the module available (GitHub Actions via `west.yml`, or locally via `-DZMK_EXTRA_MODULES=".../zmk-keypeek-layer-notifier"`).
3. Ensure your build includes `zmk-raw-hid` and has `CONFIG_RAW_HID=y`.
