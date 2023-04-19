#include "state.h"

#include "freertos/FreeRTOS.h"
#include "i2c_helper.h"

const char* status_name[] = {
  /* STATUS_SPRUNG  */ "sprung",
  /* STATUS_SET     */ "set",
  /* STATUS_UNKNOWN */ "unknown",
};

const char* event_name[] = {
  /* EVENT_SPRUNG    */ "sprung",
  /* EVENT_SET       */ "set",
  /* EVENT_HEARTBEAT */ "heartbeat",
  /* EVENT_NONE      */ "none",
};

event_t event_from_status_transition(const status_t old_status, const status_t new_status) {
  if (old_status != new_status) {
    if (new_status == STATUS_SPRUNG) {
      return EVENT_SPRUNG;
    }

    if (new_status == STATUS_SET) {
      return EVENT_SET;
    }
  }

  return EVENT_NONE;
}

RTC_DATA_ATTR status_t status = STATUS_UNKNOWN;
RTC_DATA_ATTR time_t previous_message_time = 0;

i2c_port_t axp_handle = I2C_NUM_0;
axp192_t axp = {
  .read = &i2c_read,
  .write = &i2c_write,
  .handle = &axp_handle,
};
