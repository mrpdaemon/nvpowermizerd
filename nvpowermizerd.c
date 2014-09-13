/*
 * nvpowermizerd - a daemon to improve nVidia PowerMizer mode behavior
 * Copyright (C) 2014  Mark R. Pariente
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>

// How long should the system be idle before we switch to lower power mode
#define SWITCH_TO_LOW_POWER_AFTER_IDLE_MS (20000)

/*
 * When in power saving mode we need to react very quickly to user input
 * so we poll frequently
 */
#define IDLE_POLL_FREQ_LOW_POWER_MS (10)

/*
 * When in high power mode we take it easy when polling to save CPU - this is
 * the common case.
 */
#define IDLE_POLL_FREQ_HIGH_POWER_MS (5000)

// Whether we're driving the card in high power or low power mode.
typedef enum Mode {
   LOW_POWER,
   HIGH_POWER,
} Mode;

Mode currentMode = LOW_POWER;

// Returns the idle time in milliseconds as reported by X
time_t GetIdleTimeMS () {
   XScreenSaverInfo *SSInfo;
   Display *display;
   time_t idleTime;
   int screen;

   SSInfo = XScreenSaverAllocInfo();
   if ((display = XOpenDisplay(NULL)) == NULL) {
      return -1;
   }
   screen = DefaultScreen(display);

   XScreenSaverQueryInfo(display, RootWindow(display,screen), SSInfo);
   idleTime = SSInfo->idle;

   XFree(SSInfo);
   XCloseDisplay(display); 

   return idleTime;
}

void SwitchToLowPower()
{
   int ret;

   ret = system("nvidia-settings -a [gpu:0]/GPUPowerMizerMode=0");

   printf("Switched to low power (ret = %d) - polling for action every %d ms\n",
          ret, IDLE_POLL_FREQ_LOW_POWER_MS);

   currentMode = LOW_POWER;
}

void SwitchToHighPower()
{
   int ret;

   ret = system("nvidia-settings -a [gpu:0]/GPUPowerMizerMode=1");

   printf("Switched to high power (ret = %d) - polling for idle every %d ms\n",
          ret, IDLE_POLL_FREQ_HIGH_POWER_MS);

   currentMode = HIGH_POWER;
}

int main()
{
   int idleMS;

   while (1) {
      idleMS = GetIdleTimeMS();

#ifdef DEBUG
      printf("Poll - idle time: %d ms Mode: %s\n", idleMS,
              currentMode == LOW_POWER ? "Low power" : "High power");
#endif

      if (currentMode == LOW_POWER) {
         if (idleMS < SWITCH_TO_LOW_POWER_AFTER_IDLE_MS) {
            SwitchToHighPower();
         }
      } else {
         if (idleMS >= SWITCH_TO_LOW_POWER_AFTER_IDLE_MS) {
            SwitchToLowPower();
         }
      }

      usleep(currentMode == LOW_POWER ? IDLE_POLL_FREQ_LOW_POWER_MS * 1000 :
                                        IDLE_POLL_FREQ_HIGH_POWER_MS * 1000);
   }
}
