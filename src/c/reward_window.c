#include <pebble.h>
#include "reward_window.h"
#include "state.h"
#include "fireworks.h"

static Window *s_goal_window;
static Layer *s_reward_layer;
static BitmapLayer *render_layer = NULL;
static GBitmap *render_bitmap = NULL;
static AppTimer *reward_timer;

static void reward_complete_timer() {
  window_stack_remove(s_goal_window, true);
}

static void fireworks_timer(void* data) {
  app_timer_register(20, fireworks_timer, data);
  Firework_Update(render_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(render_layer));
}

static void reward_update_proc(Layer *layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  char main_text[32] = {0};
  snprintf(main_text, sizeof(main_text), "%d", state.settings.steps_target_daily);
  
  // Add daily step number
  GFont big_font = fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
  GSize txt_sz = graphics_text_layout_get_content_size(main_text, big_font, GRect(0,0,bounds.size.w,50), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  GRect txt_frame = GRect(center.x - (txt_sz.w/2), center.y - 20, txt_sz.w, txt_sz.h);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, main_text, big_font, txt_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void reward_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  //window_set_background_color(window, state.settings.color_background);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Setup render layer for fireworks
  Firework_Initialize(bounds.size.w, bounds.size.h);
  
  render_layer = bitmap_layer_create(bounds);
  render_bitmap = gbitmap_create_blank(bounds.size, GBitmapFormat8Bit);
  bitmap_layer_set_bitmap(render_layer, render_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(render_layer));
  
  s_reward_layer = layer_create(bounds);
  layer_set_update_proc(s_reward_layer, reward_update_proc);
  layer_add_child(window_layer, s_reward_layer);
  
  fireworks_timer(NULL);
  vibes_long_pulse();
  
  layer_mark_dirty(s_reward_layer);
}

static void reward_window_unload(Window *window) {
  bitmap_layer_destroy(render_layer);
  layer_destroy(s_reward_layer);
  s_reward_layer = NULL;
  app_timer_cancel(reward_timer);
}

void reward_window_push(uint16_t seconds_to_display) {
  bool quiet_time_on = state.settings.respect_quiet_time && quiet_time_is_active();
  if(!quiet_time_on) {
    s_goal_window = window_create();
    window_set_window_handlers(s_goal_window, (WindowHandlers) {
      .load = reward_window_load,
      .unload = reward_window_unload,
    });
    
    reward_timer = app_timer_register(1000 * seconds_to_display, reward_complete_timer, NULL);
    
    window_stack_push(s_goal_window, true);
  }
}