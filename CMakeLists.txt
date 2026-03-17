if(CONFIG_ZMK_KEYPEEK_LAYER_NOTIFIER)
  if(NOT CONFIG_RAW_HID)
    message(FATAL_ERROR "zmk-keypeek-layer-notifier requires CONFIG_RAW_HID=y. Add zmk-raw-hid to your west projects list.")
  endif()

  target_sources(app PRIVATE src/layer_notifier.c)
endif()
