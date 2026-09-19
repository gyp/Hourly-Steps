#ifndef STATE_H
#define STATE_H

#include <pebble.h>
#include <stdbool.h>

// Constants
#define REWARD_SCREEN_DURATION 10

// Wakeup Persist Keys
#define WAKEUP_REMINDER_ID 101
#define WAKEUP_REMINDER_REASON 102
#define WAKEUP_HOURLY_ID 103
#define WAKEUP_HOURLY_REASON 104

// Steps Persist Keys
#define PERSIST_KEY_LAST_STEPS 201
#define PERSIST_KEY_STEP_STATE 202
#define PERSIST_KEY_DAILY_STEPS_COMPLETE 203

// Settings Persist Keys
#define SETTINGS_KEY 301

// Steps-related state
typedef struct {
  int goal_stretch;
  int goal_stretch_hourly;
  int goal_stretch_total;
  int current_day;
  int current_hour;
  int current_start_hour;
} steps_state_t;

// Time-related state
typedef struct {
  time_t active_start_time;
  time_t active_end_time;
  time_t now;
  time_t start_of_hour;
  int active_hours_complete;
  int active_hours_remaining;
} time_state_t;

// Settings
typedef struct { 
  int steps_target_daily;
  int steps_target_minimum;
  
  int time_start_hour;
  int time_active_hours;
  int time_reminder_minutes;
  bool time_active_days[7];

  bool use_fast_refresh;
  bool respect_quiet_time;
  bool use_background_worker;
  bool use_steps_reward;
  
  // Everything above here is the "worker-visible prefix": Background worker mirrors it in its own
  // struct and reads it back with a short persist_read_data.  Adding/reordering anything above
  // means updating worker.c too.
  //
  // Everything below here is the "app-only tail".  Colors live here because the worker has no use
  // for them and can't understand "GColor" anyways.  Note that settings are persisted as a raw
  // struct with no versioning, so new fields must be appended at the very bottom to keep the
  // existing fields at their current byte offsets for users upgrading from an older build.
  GColor color_background;
  GColor color_text;
  GColor color_notch_background;
  GColor color_notch_text;
  GColor color_steps_track;
  GColor color_steps_progress;

  bool display_time;
} settings_state_t;

// Global state container
typedef struct {
  steps_state_t steps;
  time_state_t time;
  settings_state_t settings;
} app_state_t;

// Extern declarations
extern app_state_t state;

#endif // STATE_H
