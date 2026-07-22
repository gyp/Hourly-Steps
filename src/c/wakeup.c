#include <pebble.h>
#include "state.h"
#include "calculations.h"

AppTimer *timeout_timer;

// Close the app on timeout
static void timeout() {
  window_stack_pop_all(false);
}

void cancel_timeout() {
  if(timeout_timer != NULL) {
    app_timer_cancel(timeout_timer);
  }
}

void schedule_timeout() {
  // Schedule a timer to automatically close the app after reminder time period is up
  timeout_timer = app_timer_register(state.settings.time_reminder_minutes * SECONDS_PER_MINUTE * 1000, timeout, NULL);
}

// Called by the system when the app receives a wakeup (or scheduled check).
// Returns a bool that determine whether window is created or not
bool wakeup_maybe_notify() {
  // Ensure times/steps are fresh
  calculations_refresh();

  // Respect quiet time if requested
  if (state.settings.respect_quiet_time && quiet_time_is_active()) {
    return false;
  }

  // Only notify during active hours
  time_t now = time(NULL);
  struct tm t = *localtime(&now);
  if (!state.settings.time_active_days[t.tm_wday] || state.time.active_start_time > now || state.time.active_end_time <= now) {
    return false;
  }

  // If current_hour is less than minimum, vibrate once and attempt to bring attention
  if (state.steps.current_hour < state.settings.steps_target_minimum) {
    // Buzz user to remind them to move
    vibes_short_pulse();
    
    schedule_timeout();
    
    return true;
  }
  return false;
}

time_t next_hour() {
  time_t now = time(NULL);
  struct tm *nxthr = localtime(&now);
  nxthr->tm_hour++;
  nxthr->tm_min = 0;
  nxthr->tm_sec = 0;
  return mktime(nxthr);
}

time_t next_reminder() {
  time_t now = time(NULL);
  // The number of seconds after start of hour to send a reminder
  int reminder_seconds = (MINUTES_PER_HOUR - state.settings.time_reminder_minutes) * SECONDS_PER_MINUTE;
  if(state.time.active_end_time <= now) { // Active Hours complete
    // It's after active time ended, next reminder is the next day
    return state.time.active_start_time + SECONDS_PER_DAY + reminder_seconds;
  } else if (state.time.active_start_time > now) {
    // It's before active time starts, next reminder is the start of active time
    return state.time.active_start_time + reminder_seconds;
  }
  // Next reminder is next hour, adjusted by reminder minutes
  return next_hour() + reminder_seconds;
}

void schedule_hourly() {
  if(persist_exists(WAKEUP_HOURLY_ID) && wakeup_query(persist_read_int(WAKEUP_HOURLY_ID), NULL)) {
    // Wakeup already scheduled
    return;
  }
  
  WakeupId id = wakeup_schedule(next_hour(), WAKEUP_HOURLY_REASON, false);
  persist_write_int(WAKEUP_HOURLY_ID, (int)id);
}

void schedule_next_reminder() {
  if(persist_exists(WAKEUP_REMINDER_ID) && wakeup_query(persist_read_int(WAKEUP_REMINDER_ID), NULL)) {
    return;
  }
  
  WakeupId id = wakeup_schedule(next_reminder(), WAKEUP_REMINDER_REASON, false);
  persist_write_int(WAKEUP_REMINDER_ID, (int)id);
}

bool process_wakeup(WakeupId id, int32_t reason) {
  if(reason == WAKEUP_REMINDER_REASON) {
    // Handle wakeup for hourly steps reminder
    schedule_next_reminder();
    return wakeup_maybe_notify();
  } else if (reason == WAKEUP_HOURLY_REASON) {
    // Due to a Pebble limitation: health_service_sum doesn't provide accurate values at an hourly basis
    // So instead we set a timer and get the current steps each hour on the hour and persist it
    set_hourly_steps();
    schedule_hourly();
  }
  return false;
}

void wakeup_handler(WakeupId id, int32_t reason) {
  process_wakeup(id, reason);
}

// Set wakeup timers
bool wakeup_init() {
  // Used to handle wakeup when app is already running
  wakeup_service_subscribe(wakeup_handler);
  
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // Run these processes only if app was woken by system
    WakeupId id = 0;
    int32_t reason = 0;
    wakeup_get_launch_event(&id, &reason); // Writes wakeup reason to id & reason
    
    return process_wakeup(id, reason);
  }
  schedule_hourly();
  schedule_next_reminder();
  return true;
}