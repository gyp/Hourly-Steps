#include <pebble.h>
#include "state.h"
#include "calculations.h"
#include "wakeup.h"
#include "configuration.h"
#include "window.h"

static void init(void) { 
  // Pull in Config Settings from App
  config_init();
  
  // Update the actual live data
  calculations_refresh();
  
  if(state.settings.use_background_worker) {
    // Run Background worker if it's not already running
    if(!app_worker_is_running()) {
      app_worker_launch();
    }
    if(launch_reason() == APP_LAUNCH_WORKER && state.settings.respect_quiet_time && quiet_time_is_active()) {
      // Worker can't access quiet_time_is_active and therefore can't know if it's active or not
      // So we have to block app launches here instead.  Unfortunately this does mean that it can
      // close down foreground apps unintentionally in these moments
      return;
    }
  } else {
    // If wakeup init returns false, then do not construct window
    if(!wakeup_init()){
      return;
    }
  }
  window_push();
}

static void deinit(void) {
  window_pop();
  config_deinit();
  cancel_timeout();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
