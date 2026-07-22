#include "calculations.h"
#include "state.h"
#include <pebble.h>

// Define the single instances of globals here
app_state_t state;

// Helper: build a time_t for today's date at given hour
static time_t make_time_for_hour(int hour) {
  if(hour > 23) hour = 23;
  if(hour < 0) hour = 0;
  time_t now = time(NULL);
  struct tm t = *localtime(&now);
  t.tm_hour = hour;
  t.tm_min = 0;
  t.tm_sec = 0;
  return mktime(&t);
}

void calculations_init_defaults() {
  state.steps.goal_stretch = 0;
  state.steps.goal_stretch_total = 0;

  state.time.now = time(NULL);
  state.time.start_of_hour = state.time.now;
  state.time.active_hours_complete = 0;
  state.time.active_hours_remaining = state.settings.time_active_hours;
  state.steps.current_day = 0;
  state.steps.current_hour = 0;
  state.steps.current_start_hour = 0;
}

void set_hourly_steps() {
  persist_write_int(PERSIST_KEY_LAST_STEPS, health_service_sum_today(HealthMetricStepCount));
  time_t now = time(NULL);
  struct tm t = *localtime(&now);
  if(t.tm_hour == 0) {
    persist_write_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE, false);
  }
}

void calculations_update_time() {
  state.time.now = time(NULL);
  state.time.active_start_time = make_time_for_hour(state.settings.time_start_hour);
  state.time.active_end_time = make_time_for_hour(state.settings.time_start_hour + state.settings.time_active_hours);

  struct tm tm_now = *localtime(&state.time.now);
  tm_now.tm_min = 0;
  tm_now.tm_sec = 0;
  state.time.start_of_hour = mktime(&tm_now);

  state.time.active_start_time = make_time_for_hour(state.settings.time_start_hour);
  state.time.active_end_time = make_time_for_hour(state.settings.time_start_hour + state.settings.time_active_hours);
  
  if (state.time.active_start_time <= state.time.now && state.time.active_end_time > state.time.now) {
    int seconds_elapsed = state.time.now - state.time.active_start_time;
    int seconds_remaining = state.time.active_end_time - state.time.now;

    state.time.active_hours_complete = seconds_elapsed / SECONDS_PER_HOUR;
    state.time.active_hours_remaining = seconds_remaining / SECONDS_PER_HOUR;
  } else if (state.time.active_start_time < state.time.now) {
    state.time.active_hours_complete = state.settings.time_active_hours;
    state.time.active_hours_remaining = 0;
  } else {
    state.time.active_hours_complete = 0;
    state.time.active_hours_remaining = state.settings.time_active_hours;
  }
}

void calculations_update_steps() {
  state.steps.current_day = health_service_sum_today(HealthMetricStepCount);
  state.steps.current_start_hour = persist_read_int(PERSIST_KEY_LAST_STEPS);
  state.steps.current_hour = state.steps.current_day - state.steps.current_start_hour;
  
  if(state.steps.current_hour < 0) {
    // There's a bug where PERSIST_KEY_LAST_STEPS doesn't load properly and causes hourly steps to be negative
    // Not sure what causes it, but can at least stop it from being negative.  It gets fixed on it's own next hour generally.
    state.steps.current_hour = 0;
  }

  int remaining_steps_needed = state.settings.steps_target_daily - state.steps.current_start_hour;
  if (remaining_steps_needed < 0) remaining_steps_needed = 0;
  state.steps.goal_stretch = remaining_steps_needed / (state.time.active_hours_remaining + 1); // Add 1 since hours remaining doesn't count current
  state.steps.goal_stretch_hourly = state.settings.steps_target_daily / state.settings.time_active_hours;

  state.steps.goal_stretch_total = ((float)state.time.active_hours_complete + 1.0) * (float)state.settings.steps_target_daily / (float)state.settings.time_active_hours;
  if(state.steps.goal_stretch_total > state.settings.steps_target_daily) state.steps.goal_stretch_total = state.settings.steps_target_daily;
}

// Used to handle calculations of things that need to be rebuilt every tick
void calculations_refresh() {
  calculations_update_time();
  calculations_update_steps();
}