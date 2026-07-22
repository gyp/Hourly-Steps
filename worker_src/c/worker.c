#include <pebble_worker.h>

// Worker can't include any other files because it can't include pebble.h, so we need to duplicate functionality

// These keys must match the foreground app keys
#define PERSIST_KEY_LAST_STEPS 201
#define PERSIST_KEY_DAILY_STEPS_COMPLETE 203

#define SETTINGS_KEY 301

// This struct needs to match the order of the state.h one to work properly
// We can cut off things we don't need if they're at the bottom, like GColor attributes
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
} prv_settings_state_t;

prv_settings_state_t settings;

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

static void set_hourly_steps() {
  persist_write_int(PERSIST_KEY_LAST_STEPS, health_service_sum_today(HealthMetricStepCount));
}

static void load_settings() {
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

bool wakeup_maybe_notify() {
  // Only notify during active hours
  time_t now = time(NULL);
  struct tm t = *localtime(&now);
  time_t active_start_time = make_time_for_hour(settings.time_start_hour);
  time_t active_end_time = make_time_for_hour(settings.time_start_hour + settings.time_active_hours);
  if (!settings.time_active_days[t.tm_wday] || active_start_time > now || active_end_time <= now) {
    return false;
  }

  // If current_hour is less than minimum, vibrate once and attempt to bring attention
  int current_start_hour = persist_read_int(PERSIST_KEY_LAST_STEPS);
  int current_hour = health_service_sum_today(HealthMetricStepCount) - current_start_hour;
  if (current_hour < settings.steps_target_minimum) {
    return true;
  }
  return false;
}

static void check_steps() {
  bool goal_met = persist_read_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE);
  if(settings.use_steps_reward && health_service_sum_today(HealthMetricStepCount) >= settings.steps_target_daily && !goal_met) {
    // Daily steps met, launch foreground for reward animation
    persist_write_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE, true);
    worker_launch_app();
    psleep(1000); // Wait a sec before sending message to give foreground app time to boot
    AppWorkerMessage message = {}; // Don't need to send anything, the ping is enough right now
    app_worker_send_message(1, &message);
  }
}

// Handler for health events, the only type we care about are steps
static void health_updates(HealthEventType eventType, void *context) {
  switch (eventType) {
    case HealthEventSignificantUpdate:
    case HealthEventMovementUpdate:
      check_steps();
      break;
    default:
      break;
  }
}

static void minute_handler(struct tm *tick_time, TimeUnits units_changed) {
  load_settings();
  if(tick_time->tm_min == 0) {
    // If start of hour, set hourly steps
    set_hourly_steps();
    if(tick_time->tm_hour == 0) {
      // If start of day, reset goal met
      persist_write_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE, false);
    }
    
    if(settings.use_fast_refresh) {
      // Send a message to foreground to tell it to rerender if it's already open
      // Only necessary with fast refresh because the minute refreshes will handle it otherwise
      AppWorkerMessage message = {}; // Don't need to send anything, the ping is enough right now
      app_worker_send_message(2, &message); 
    }
  } else if(tick_time->tm_min == 60 - settings.time_reminder_minutes && wakeup_maybe_notify()) {
    // Otherwise check if we should do a reminder
    AppWorkerMessage message = {}; // Don't need to send anything, the ping is enough right now
    worker_launch_app();
    psleep(1000); // Wait a sec before sending message to give foreground app time to boot
    app_worker_send_message(0, &message); // Send a message in case app is already open
  }
}

static void worker_init() {
  load_settings();
  tick_timer_service_subscribe(MINUTE_UNIT, minute_handler);
  health_service_events_subscribe(health_updates, NULL);
}

static void worker_deinit() {
  tick_timer_service_unsubscribe();
  health_service_events_unsubscribe();
}

int main(void) {
  worker_init();
  worker_event_loop();
  worker_deinit();
}