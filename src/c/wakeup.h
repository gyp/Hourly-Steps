#include <pebble.h>
#include "state.h"

bool wakeup_init();
bool wakeup_maybe_notify();
void cancel_timeout();
void schedule_timeout();