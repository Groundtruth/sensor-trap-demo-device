#pragma once

#include <time.h>
#include "axp192.h"

typedef enum status_t {
  STATUS_SPRUNG = 0,
  STATUS_SET,
  STATUS_UNKNOWN,
} status_t;

extern const char* status_name[];

typedef enum event_t {
  EVENT_SPRUNG = 0,
  EVENT_SET,
  EVENT_HEARTBEAT,
  EVENT_NONE,
} event_t;

extern const char* event_name[];

event_t event_from_status_transition(const status_t old_status, const status_t new_status);

extern status_t status;
extern time_t previous_message_time;

extern axp192_t axp;