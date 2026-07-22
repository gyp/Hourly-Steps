#include <pebble.h>
#include "configuration.h"
#include "calculations.h"
#include "wakeup.h"
#include "state.h"

static void load_settings() {
  persist_read_data(SETTINGS_KEY, &state.settings, sizeof(state.settings));
}

static void save_settings() {
  persist_write_data(SETTINGS_KEY, &state.settings, sizeof(state.settings));
}

static void swap_timer_type() {
  if(state.settings.use_background_worker) {
    // Cancel wakeup timers and start background worker instead
    wakeup_cancel_all();
    app_worker_launch();
  } else {
    // Kill background worker and start timers instead
    app_worker_kill();
    wakeup_init();
  }
}

static void handle_inbox_received(DictionaryIterator *iter, void *context) {
  bool need_timer_swap = false;
  
  // Load Steps
  Tuple *target_daily_steps_t = dict_find(iter, MESSAGE_KEY_target_daily_steps);
  if(target_daily_steps_t) {
    state.settings.steps_target_daily = target_daily_steps_t->value->int32;
  }
  Tuple *min_per_hour_t = dict_find(iter, MESSAGE_KEY_min_per_hour);
  if(min_per_hour_t) {
    state.settings.steps_target_minimum = min_per_hour_t->value->int32;
  }
  
  // Load Timing
  Tuple *start_hour_t = dict_find(iter, MESSAGE_KEY_start_hour);
  if(start_hour_t) {
    state.settings.time_start_hour = start_hour_t->value->int32;
  }
  Tuple *active_hours_t = dict_find(iter, MESSAGE_KEY_active_hours);
  if(active_hours_t) {
    state.settings.time_active_hours = active_hours_t->value->int32;
  }
  Tuple *minutes_before_t = dict_find(iter, MESSAGE_KEY_minutes_before);
  if(minutes_before_t) {
    state.settings.time_reminder_minutes = minutes_before_t->value->int32;
  }
  for (int i = 0; i < 7; i++) {
    Tuple *t = dict_find(iter, MESSAGE_KEY_active_days + i);
    if (t) {
      state.settings.time_active_days[i] = t->value->uint16 == 1;
    }
  }
  
  // Load Misc
  Tuple *use_fast_refresh_t = dict_find(iter, MESSAGE_KEY_use_fast_refresh);
  if(use_fast_refresh_t) {
    state.settings.use_fast_refresh = use_fast_refresh_t->value->int32 == 1;
  }
  Tuple *respect_quiet_time_t = dict_find(iter, MESSAGE_KEY_respect_quiet_time);
  if(respect_quiet_time_t) {
    state.settings.respect_quiet_time = respect_quiet_time_t->value->int32 == 1;
  }
  Tuple *use_background_worker_t = dict_find(iter, MESSAGE_KEY_use_background_worker);
  if(use_background_worker_t) {
    bool old_state = state.settings.use_background_worker;
    state.settings.use_background_worker = use_background_worker_t->value->int32 == 1;
    if(old_state != state.settings.use_background_worker) {
      need_timer_swap = true;
    }
  }
  Tuple *use_steps_reward_t = dict_find(iter, MESSAGE_KEY_use_steps_reward);
  if(use_steps_reward_t) {
    state.settings.use_steps_reward = use_steps_reward_t->value->int32 == 1;
  }
  
  // Load Colors
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_color_background);
  if(bg_color_t) {
    state.settings.color_background = GColorFromHEX(bg_color_t->value->int32);
  }
  Tuple *text_color_t = dict_find(iter, MESSAGE_KEY_color_text);
  if(text_color_t) {
    state.settings.color_text = GColorFromHEX(text_color_t->value->int32);
  }
  Tuple *notch_bg_color_t = dict_find(iter, MESSAGE_KEY_color_notch_background);
  if(notch_bg_color_t) {
    state.settings.color_notch_background = GColorFromHEX(notch_bg_color_t->value->int32);
  }
  Tuple *notch_text_color_t = dict_find(iter, MESSAGE_KEY_color_notch_text);
  if(notch_text_color_t) {
    state.settings.color_notch_text = GColorFromHEX(notch_text_color_t->value->int32);
  }
  Tuple *steps_track_color_t = dict_find(iter, MESSAGE_KEY_color_steps_track);
  if(steps_track_color_t) {
    state.settings.color_steps_track = GColorFromHEX(steps_track_color_t->value->int32);
  }
  Tuple *steps_progress_color_t = dict_find(iter, MESSAGE_KEY_color_steps_progress);
  if(steps_progress_color_t) {
    state.settings.color_steps_progress = GColorFromHEX(steps_progress_color_t->value->int32);
  }
  
  save_settings();
  
  if(need_timer_swap) {
    swap_timer_type();
  }
}

static void config_defaults() {
  state.settings.steps_target_daily = 10000;
  state.settings.steps_target_minimum = 250;
  
  state.settings.time_start_hour = 9;
  state.settings.time_active_hours = 12;
  state.settings.time_reminder_minutes = 15;
  
  state.settings.color_background = GColorWhite;
  state.settings.color_text = GColorBlack;
  state.settings.color_notch_background = GColorBlue;
  state.settings.color_notch_text = GColorWhite;
  state.settings.color_steps_track = GColorLightGray;
  state.settings.color_steps_progress = GColorDarkGreen;
  
  state.settings.use_fast_refresh = true;
  state.settings.respect_quiet_time = true;
  state.settings.use_background_worker = true;
  state.settings.use_steps_reward = true;
  
  for (int i = 0; i < 7; i++) {
    state.settings.time_active_days[i] = true;
  }
}

void config_init() {
  // Set some defaults, in case app config fails for some reason
  config_defaults();
  
  // Load App Config from persistent storage
  if(persist_exists(SETTINGS_KEY)) {
    load_settings();
  }
  
  calculations_init_defaults();

  app_message_register_inbox_received(handle_inbox_received);
  app_message_open(256, 128);
}

void config_deinit() {
  app_message_deregister_callbacks();
}