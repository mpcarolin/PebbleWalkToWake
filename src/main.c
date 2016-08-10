#include <pebble.h>
#include <stdlib.h>
#include <time.h>

// indicates only available in this file! (private, basically)
static Window* s_main_window;
static TextLayer* s_time_layer;
static time_t alarm_time;


/* window init and deinit of subelement steps */
static void main_window_load(Window* window) {
  // get info about window
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // create text layer with specific bounds
  s_time_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
    
  // improve layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    
  // Add it as a child layer to the Window's root layer
  
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
}

static void main_window_unload(Window* window) {
  // reclaim memory
  text_layer_destroy(s_time_layer); 
}

static void update_time() {
  // get a tm structure
  time_t temp = time(NULL);
  struct tm* tick_time = localtime(&temp);
  
  // write current hours and minutes into buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
          "%H:%M" : "%I:%M", tick_time);
  
  // display this time on text layer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm* tick_time, TimeUnits units_changed) {
  update_time();
  // any other animations here
}

/* 
 * watch app initialization and deinitialization steps 
 */
static void init() {
  // create main window element and assign to pointer
  s_main_window = window_create();
  
  // set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
    // register with ticktimerservice
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler); 
  
  // show the window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // make sure time is displayed from start
  update_time();
  
  // set alarm time
  //TODO: make this selected and assigned in first gui window
  alarm_time = time(NULL);
  
}

/* UTILITY FUNCTIONS */

static struct tm* makeAlarmTimeStruct(int hour, int minutes, int seconds) {
  /* gather date/time elements that user does not choose */
  time_t t;
  time(&t);  // put default time elements into t
  struct tm* wakeTime = gmtime(&t);  
  
  /* change user chosen values */
  wakeTime->tm_sec = seconds;
  wakeTime->tm_min = minutes;
  wakeTime->tm_hour = hour;
  
  return wakeTime;
}

static time_t makeAlarmTime(int hour, int minutes, int seconds) {
  struct tm* wakeTime = makeAlarmTimeStruct(hour, minutes, seconds);
  time_t t = mktime(wakeTime);
  free(wakeTime);
   
  return t;
}

/* returns -1 if health data is not available */
static int getStepCount(time_t start, time_t end) {
  int count = -1;
  #if defined(PBL_HEALTH)
  HealthMetric metric = HealthMetricStepCount;
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
  
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    count = (int)health_service_sum_today(metric);
  }
  #endif
  
  return count;
}

/* vibration alarm that continues until 10 steps are taken */
static void initAlarm() {
  int startingSteps = 0;
  int currentSteps = startingSteps;
  time_t startTime = time(NULL);
  
  // stop alarm when either 90 seconds has passed or user has walked 10 steps
  int numTries = 0;
  while ((currentSteps - startingSteps) < 10 && numTries < 45) {
    vibes_long_pulse();  // vibrate 
    
    // update step count
    currentSteps = getStepCount(startTime, time(NULL));
    if (currentSteps < 0) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm cannot start. Health data is not available");
    }
    numTries++;          // continue for at least 90 seconds
    psleep(2 * 1000);    // sleep for 2 seconds = 2000ms
  }  
}

static void deinit() {
  // reclaim memory
  window_destroy(s_main_window);
}

int main() {
  /* testing 
  struct tm* t = makeAlarmTimeStruct(7, 30, 00);
  printf("Hour: %d Minute: %d \n", t->tm_hour, t->tm_min);
  */
  
  init();
  app_event_loop();
  deinit();
}