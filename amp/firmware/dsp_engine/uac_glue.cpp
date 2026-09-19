// Registers the audio class driver (compiled from audio_device.c) with the prebuilt
// TinyUSB device stack. Lives in its own translation unit because the Arduino prototype
// generator mangles `extern "C" usbd_class_driver_t const*` in a .ino.
#include "uac_config.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "class/audio/audio_device.h"

static const usbd_class_driver_t s_audioDriver = {
  .name            = "AUDIO",
  .init            = audiod_init,
  .deinit          = audiod_deinit,
  .reset           = audiod_reset,
  .open            = audiod_open,
  .control_xfer_cb = audiod_control_xfer_cb,
  .xfer_cb         = audiod_xfer_cb,
  .xfer_isr        = audiod_xfer_isr,
  .sof             = audiod_sof_isr,
};

extern "C" const usbd_class_driver_t* usbd_app_driver_get_cb(uint8_t* count) {
  *count = 1;
  return &s_audioDriver;
}
