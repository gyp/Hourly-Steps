#include <pebble.h>
#include "state.h"
#include "window.h"
#include "calculations.h"
#include "reward_window.h"
#include "wakeup.h"

// UI globals
static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_icon_bitmap;
static BitmapLayer *s_bitmap_layer;

// Represents the state that is complete
typedef enum {
  NONE = 0,
  MIN,
  GOAL,
  NUM_STATES
} State;

static const uint32_t ICON_RESOURCES[NUM_STATES] = {
  0, // Just meant to keep index parity with State enum
  RESOURCE_ID_STAR_EMPTY,
  RESOURCE_ID_STAR_FULL,
};

static State step_state = NONE;

// Icon geometry, shared with draw_time so the clock can sit exactly where the icon would be
#define ICON_SIZE 48
#define ICON_OFFSET_Y -70
#define ICON_CENTER_OFFSET_Y (ICON_OFFSET_Y + (ICON_SIZE / 2))
// graphics_text_layout_get_content_size reports a box with the font's ascent padding baked in, so
// the glyphs sit low inside it.  This lines the visible digits up with the centre of the icon.
#define TIME_GLYPH_NUDGE_Y 5

// Ensure MIN macro exists
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

// Draw helpers
static int32_t step_to_angle(int steps_value, int target_steps) {
  int steps = MIN(steps_value, target_steps);
  int steps_degrees = steps * 360 / target_steps;
  return steps == target_steps? TRIG_MAX_ANGLE : DEG_TO_TRIGANGLE(steps_degrees);
}

static void draw_icon(void) {
  // Unload current icon
  if (s_icon_bitmap) {
    gbitmap_destroy(s_icon_bitmap);
    s_icon_bitmap = NULL;
  }
  
  if (!s_bitmap_layer) {
    return;
  }

  if(step_state != NONE) {
    s_icon_bitmap = gbitmap_create_with_resource(ICON_RESOURCES[step_state]);
    bitmap_layer_set_bitmap(s_bitmap_layer, s_icon_bitmap);
  } else {
    // Has to be cleared too, otherwise the layer keeps rendering the bitmap we just destroyed
    bitmap_layer_set_bitmap(s_bitmap_layer, NULL);
  }
}

static void draw_background(GContext* ctx, GRect bounds) {
  graphics_context_set_fill_color(ctx, state.settings.color_background);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void draw_arc(GContext* ctx, GRect bounds) {
  // Circle for "Track"
  //GRect arc_bounds = GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h);
  GRect arc_bounds = grect_inset(bounds, GEdgeInsets(12));
  graphics_context_set_stroke_width(ctx, 14);
  graphics_context_set_stroke_color(ctx, state.settings.color_steps_track);
  graphics_draw_arc(ctx, arc_bounds, GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);
  
  // Circle to fill current steps
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_stroke_color(ctx, state.settings.color_steps_progress);
  graphics_draw_arc(ctx, arc_bounds, GOvalScaleModeFitCircle, 0, step_to_angle(state.steps.current_day, state.settings.steps_target_daily));

  // Notch for goal_stretch_total
  GPoint pos = gpoint_from_polar(arc_bounds, GOvalScaleModeFitCircle, step_to_angle(state.steps.goal_stretch_total, state.settings.steps_target_daily));
  graphics_context_set_fill_color(ctx, state.settings.color_notch_background);
  graphics_fill_circle(ctx, pos, 10);

  // Draw white "G" centered in notch
  graphics_context_set_text_color(ctx, state.settings.color_notch_text);
  GFont font_g = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  graphics_draw_text(ctx, "G", font_g, GRect(pos.x - 10, pos.y - 10, 20, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_time(GContext* ctx, GRect bounds) {
  // The icon and the clock share the slot above the step count, so the clock is only drawn when
  // there is no icon.  That covers the case the app is launched on its own to nag about steps.
  if (!state.settings.display_time || step_state != NONE) {
    return;
  }

  char timebuf[8];
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  bool is_24h = clock_is_24h_style();
  strftime(timebuf, sizeof(timebuf), is_24h ? "%H:%M" : "%I:%M", tm_now);
  char *time_text = timebuf;
  if (!is_24h && time_text[0] == '0') time_text++; // "02:32" -> "2:32"

  GPoint center = grect_center_point(&bounds);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GSize time_sz = graphics_text_layout_get_content_size(time_text, font, GRect(0,0,bounds.size.w,30), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  GRect time_frame = GRect(center.x - (time_sz.w/2), center.y + ICON_CENTER_OFFSET_Y - (time_sz.h/2) - TIME_GLYPH_NUDGE_Y, time_sz.w, time_sz.h);
  graphics_context_set_text_color(ctx, state.settings.color_text);
  graphics_draw_text(ctx, time_text, font, time_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_center_info(GContext* ctx, GRect bounds) {
  GPoint center = grect_center_point(&bounds);

  char main_text[32] = {0};
  int denominator = state.settings.steps_target_minimum;

  if (step_state == GOAL) {
    snprintf(main_text, sizeof(main_text), "%d", state.steps.current_hour);
  } else {
    if(step_state == MIN) {
      denominator = state.steps.goal_stretch;
    }
    snprintf(main_text, sizeof(main_text), "%d/%d", state.steps.current_hour, denominator);
  }

  // Step Count
  GFont big_font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GSize txt_sz = graphics_text_layout_get_content_size(main_text, big_font, GRect(0,0,bounds.size.w,50), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  GRect txt_frame = GRect(center.x - (txt_sz.w/2), center.y - 20, txt_sz.w, txt_sz.h);
  graphics_context_set_text_color(ctx, state.settings.color_text);
  graphics_draw_text(ctx, main_text, big_font, txt_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  
  // Surplus/Deficit
  int active_completed = state.time.active_hours_complete;
  if (active_completed < 0) active_completed = 0;
  int diff = state.steps.current_day - state.steps.goal_stretch_total;
  char diffbuf[32];
  if (diff >= 0) {
    snprintf(diffbuf, sizeof(diffbuf), "+%d Surplus", diff);
  } else {
    snprintf(diffbuf, sizeof(diffbuf), "%d Deficit", diff);
  }
  GSize diff_sz = graphics_text_layout_get_content_size(diffbuf, small_font, GRect(0,0,bounds.size.w,20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  GRect diff_frame = GRect(center.x - (diff_sz.w/2), center.y + 13, diff_sz.w, diff_sz.h);
  graphics_draw_text(ctx, diffbuf, small_font, diff_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Steps string
  char stepsbuf[16];
  snprintf(stepsbuf, sizeof(stepsbuf), "%d", state.steps.current_day);
  GSize steps_sz = graphics_text_layout_get_content_size(stepsbuf, small_font, GRect(0,0,bounds.size.w,30), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  GRect steps_frame = GRect(center.x - (steps_sz.w/2), center.y + 35, steps_sz.w, steps_sz.h);
  graphics_draw_text(ctx, stepsbuf, small_font, steps_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void canvas_update_proc(Layer *layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  draw_background(ctx, bounds);
  draw_arc(ctx, bounds);
  draw_time(ctx, bounds);
  draw_center_info(ctx, bounds);
}

static void update_steps() {
  // Update time then refresh short calculations
  state.time.now = time(NULL);
  calculations_refresh();
  
  // Determine if a threshold was just passed
  int new_step_state = step_state;
  bool quiet_time_on = state.settings.respect_quiet_time && quiet_time_is_active();
  if (step_state != MIN && state.steps.current_hour >= state.settings.steps_target_minimum && state.steps.current_hour < state.steps.goal_stretch) {
    new_step_state = MIN;
    if(!quiet_time_on){
      vibes_short_pulse();
    }
  } else if (step_state != GOAL && state.steps.current_hour >= state.settings.steps_target_minimum && state.steps.current_hour >= state.steps.goal_stretch) {
    new_step_state = GOAL;
    if(!quiet_time_on){
      vibes_double_pulse();
    }
  } else if (step_state != NONE && state.steps.current_hour < state.settings.steps_target_minimum) {
    new_step_state = NONE;
  }
  
  // If not using background worker and showing reward screen, check if daily steps passed
  if(!state.settings.use_background_worker && state.settings.use_steps_reward) {
    bool steps_complete = persist_read_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE);
    if(!steps_complete && state.steps.current_day >= state.settings.steps_target_daily) {
      persist_write_bool(PERSIST_KEY_DAILY_STEPS_COMPLETE, true);
      reward_window_push(REWARD_SCREEN_DURATION);
    }
  }
  
  // Update the icon only if it needs updating
  if(new_step_state != (int)step_state) {
    step_state = new_step_state;
    persist_write_int(PERSIST_KEY_STEP_STATE, step_state);
    draw_icon();
  }

  layer_mark_dirty(s_canvas_layer);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, state.settings.color_background);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  if(persist_exists(PERSIST_KEY_STEP_STATE)) {
    step_state = persist_read_int(PERSIST_KEY_STEP_STATE);
  }
  
  // Icon area
  GPoint center = grect_center_point(&bounds);
  s_bitmap_layer = bitmap_layer_create(GRect(center.x - (ICON_SIZE/2), center.y + ICON_OFFSET_Y, ICON_SIZE, ICON_SIZE));
  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  layer_add_child(window_get_root_layer(s_main_window), bitmap_layer_get_layer(s_bitmap_layer));
  draw_icon();

  layer_mark_dirty(s_canvas_layer);
}

static void main_window_unload(Window *window) {
  if (s_icon_bitmap) {
    gbitmap_destroy(s_icon_bitmap);
    s_icon_bitmap = NULL;
    bitmap_layer_destroy(s_bitmap_layer);
  }
  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
}

// Handler for health events, the only type we care about are steps
static void health_updates(HealthEventType eventType, void *context) {
  switch (eventType) {
    case HealthEventSignificantUpdate:
    case HealthEventMovementUpdate:
      update_steps();
      break;
    default:
      break;
  }
}

// Handling messages from the background worker
static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  bool quiet_time_on = state.settings.respect_quiet_time && quiet_time_is_active();
  if(type == 0) { // Reminder to Move
    if(!quiet_time_on) {
      vibes_short_pulse();
    }
    schedule_timeout();
  } else if (type == 1) { // Daily Steps Complete
    reward_window_push(REWARD_SCREEN_DURATION);
  } else if (type == 2) { // Re-render for new hour
    update_steps();
  }
}

// Handler for minute change if user turned off fast refresh
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_steps();
}

// Lines the service subscriptions up with the current settings.  Called when the window is built
// and again whenever a setting that affects what we need to listen to is changed.
void window_update_time_subscription(void) {
  if (!s_main_window) {
    return;
  }

  if(state.settings.use_fast_refresh) {
    health_service_events_subscribe(health_updates, NULL);
  } else {
    health_service_events_unsubscribe();
  }

  // Fast refresh only fires on movement, so the clock needs a minute tick of its own to stay current
  if(!state.settings.use_fast_refresh || state.settings.display_time) {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  } else {
    tick_timer_service_unsubscribe();
  }

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

void window_push(void) {
  if (!s_main_window) {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
      .load = main_window_load,
      .unload = main_window_unload,
    });
    window_update_time_subscription();
    
    if(state.settings.use_background_worker) {
      // Subscribe to background worker messages, so that we can handle reminders when the app is already open
      app_worker_message_subscribe(worker_message_handler);
    }
  }
  window_stack_push(s_main_window, true);
}

void window_pop(void) {
  if (s_main_window) {
    health_service_events_unsubscribe();
    tick_timer_service_unsubscribe();
    app_worker_message_unsubscribe();
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
}
