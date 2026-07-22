# Hourly-Steps
Pebble app for tracking hourly steps

Hourly Steps mimics Fitbit's "Reminder to Move" functionality but with way more user control.

Through App settings you can adjust:

- What hour to start getting reminders
- How many minutes before the end of the hour to get reminded to move
- How many hours you want to be reminded to move
- The minimum steps you want to be reminded to do each hour
- What your daily step goal is
- What days of the week you want to receive reminders
- All the colors used in the app's visual appearance

Hourly Steps will even help you reach your daily step goals by splitting that goal by your active hours and showing it as a stretch goal after hitting the minimum.  For example: if you want to do 10,000 steps a day and be active for 10 hours a day, it'll show a stretch goal of 1,000 steps each hour after finishing the minimum steps.  That stretch goal will automatically adjust if you get closer to that goal or fall behind, creating a "deficit" or "surplus" value that shows how on track you are.  You'll even get a Fireworks show upon completion of daily steps.

Please allow an hour after installing for the app to establish steps at the start of the next hour.  It may show an incorrect hourly steps value until then.

Note: Due to Pebble limitations around tracking steps within a specific time frame, this app uses a background worker to get steps at the start of each hour and determine when to open the app for reminders.  You can optionally turn this off if you need the background worker for something else.  This will cause Hourly Steps to use wakeup timers instead, however that has some limitations.  Since there can only be one Foreground app at a time, it'll close down any other app you have open even if it doesn't need to remind you of steps.  This will happen once at the start of an hour and once during active hours at the reminder minute you set (but not outside of active hours).  It may also fail to use the timers entirely if another app has scheduled a wakeup for the same minute  This should not happen with background worker active.  Because of this I highly recommend keeping the background worker active.

Fireworks code adapted from:

https://apps.repebble.com/fireworks-particle-effects-glancing_5592dc3e53d39fc50500006b
