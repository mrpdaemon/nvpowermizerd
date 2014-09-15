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

#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Argument parser / documentation
const char *argp_program_version = "nvpowermizerd DEVELOPMENT";
const char *argp_program_bug_address = "markpariente@gmail.com";
static char doc[] = "nvpowermizerd - a daemon to improve nVidia PowerMizer "
                    "mode behavior";
static struct argp_option options[] = {
   {"verbose", 'v', 0, 0, "Show debugging logs" },
   {"gpuid", 'g', "GPU-ID", 0, "GPU ID as shown by 'nvidia-settings -q gpus'"},
   {0}
};

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

// Command strings for switching to modes
#define LOW_POWER_CMD_FMT_STR  "nvidia-settings -a [gpu:%d]/GPUPowerMizerMode=0"
#define HIGH_POWER_CMD_FMT_STR  "nvidia-settings -a [gpu:%d]/GPUPowerMizerMode=1"
static char *lowPowerCmd;
static char *highPowerCmd;

// Whether we're driving the card in high power or low power mode.
typedef enum Mode {
   LOW_POWER,
   HIGH_POWER,
} Mode;

// Global state
static Mode currentMode = LOW_POWER;
static int gpuID = 0;

// verbose = 0 is LOG only, verbose = 1 also shows LOGDEBUG
static int verbose = 0;

// Debug / logging macros
#define LOGDEBUG(...)     \
   if (verbose) {         \
      printf(__VA_ARGS__);\
   }

#define LOG(...) \
   printf(__VA_ARGS__);

// Global variables for X access
static XScreenSaverInfo *SSInfo;
static Display *display;
static int screen;

// Returns the idle time in milliseconds as reported by X
inline time_t
GetIdleTimeMS()
{
   XScreenSaverQueryInfo(display, RootWindow(display,screen), SSInfo);
   return SSInfo->idle;
}

void
SwitchToLowPower()
{
   int retVal;

   retVal = system(lowPowerCmd);
   LOGDEBUG("nvidia-settings returned %d\n", retVal);

   LOG("Switched to low power - polling for action every %dms\n",
       IDLE_POLL_FREQ_LOW_POWER_MS);

   currentMode = LOW_POWER;
}

void
SwitchToHighPower()
{
   int retVal;

   retVal = system(highPowerCmd);
   LOGDEBUG("nvidia-settings returned %d\n", retVal);

   LOG("Switched to high power - polling for idle every %dms\n",
       IDLE_POLL_FREQ_HIGH_POWER_MS);

   currentMode = HIGH_POWER;
}

void
Cleanup()
{
   free(lowPowerCmd);
   free(highPowerCmd);
   XFree(SSInfo);
   XCloseDisplay(display);
}

void
HandleSignal(int signum)
{
   LOGDEBUG("Signal %d received.\n", signum);

   SwitchToLowPower();
   Cleanup();

   LOG("Exiting program.\n");
   exit(0);
}

static error_t
ParseOption(int key, char *arg, struct argp_state *state)
{
   switch (key) {
      case 'v':
         verbose = 1;
         break;
      case 'g':
         gpuID = atoi(arg);
         LOGDEBUG("GPU ID set to %d\n", gpuID);
         break;
      default:
         return ARGP_ERR_UNKNOWN;
   }

   return 0;
}

void
Init()
{
   struct sigaction action;

   // Install signal handlers
   memset(&action, 0, sizeof(action));
   action.sa_handler = HandleSignal;
   sigaction(SIGTERM, &action, NULL);
   sigaction(SIGINT, &action, NULL);

   // Initialize X state
   SSInfo = XScreenSaverAllocInfo();
   if ((display = XOpenDisplay(NULL)) == NULL) {
      LOG("Couldn't open X display!\n");
      exit(1);
   }
   screen = DefaultScreen(display);
}

int
PrepareCommands()
{
   int retVal;

   retVal = asprintf(&lowPowerCmd, LOW_POWER_CMD_FMT_STR, gpuID);
   if (retVal < 0) {
      return retVal;
   }

   retVal = asprintf(&highPowerCmd, HIGH_POWER_CMD_FMT_STR, gpuID);
   if (retVal < 0) {
      return retVal;
   }

   return 0;
}

// Parameters for the argument parser
static struct argp argpParams = {options, ParseOption, NULL, doc};

int
main(int argc, char *argv[])
{
   int idleMS;

   Init();

   // Parse arguments
   argp_parse(&argpParams, argc, argv, 0, 0, NULL);

   if (PrepareCommands() != 0) {
      LOG("Failed to allocate memory for command strings");
      return 1;
   }

   while (1) {
      idleMS = GetIdleTimeMS();

      LOGDEBUG("Poll - idle time: %dms Mode: %s\n", idleMS,
               currentMode == LOW_POWER ? "Low power" : "High power");

      if (currentMode == LOW_POWER) {
         if (idleMS < SWITCH_TO_LOW_POWER_AFTER_IDLE_MS) {
            SwitchToHighPower();

            // We don't need to poll again until the idle timeout
            LOGDEBUG("Polling again in %dms\n",
                     SWITCH_TO_LOW_POWER_AFTER_IDLE_MS - idleMS + 1);
            usleep((SWITCH_TO_LOW_POWER_AFTER_IDLE_MS - idleMS + 1) * 1000);
         } else {
            usleep(IDLE_POLL_FREQ_LOW_POWER_MS * 1000);
         }
      } else { // currentMode == HIGH_POWER

         if (idleMS >= SWITCH_TO_LOW_POWER_AFTER_IDLE_MS) {
            SwitchToLowPower();
         }

         usleep(IDLE_POLL_FREQ_HIGH_POWER_MS * 1000);
      }
   }
}
