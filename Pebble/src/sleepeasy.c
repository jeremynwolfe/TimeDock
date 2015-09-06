#include <pebble.h>

static const SmartstrapServiceId SERVICE_ID = 0x1001;
static const SmartstrapAttributeId SOUND_ATTRIBUTE_ID = 0x0001;
static const size_t SOUND_ATTRIBUTE_LENGTH = 1;
static const SmartstrapAttributeId UPTIME_ATTRIBUTE_ID = 0x0002;
static const size_t UPTIME_ATTRIBUTE_LENGTH = 4;
//Say time command: two bytes, first is hour in 24hrs., second is minutes
static const SmartstrapAttributeId SayTimeAttributeID = 0x0003;
static const size_t SayTimeAttributeIDLength = 2;
//Renotify command: Alert over serial when a notification occurs, and then goes away.
//Passes a bool, true is a new notification, false is the notification window closes.
static const SmartstrapAttributeId RenotifyAttributeId = 0x0004;
static const size_t RenotifyAttributeIdLength = 1;
//ultrasound detected something near
static const SmartstrapAttributeId ultrasoundAttributeID = 0x005;
static const size_t ultrasoundAttributeIdLength = 1;



static SmartstrapAttribute *led_attribute;
static SmartstrapAttribute *uptime_attribute;
static SmartstrapAttribute *sayTime_attribute;
static SmartstrapAttribute *renotifyAttribute;
static SmartstrapAttribute *ultrasoundAttribute;

static Window *window;
static TextLayer *status_text_layer;
static TextLayer *uptime_text_layer;


static void prv_availability_changed(SmartstrapServiceId service_id, bool available) {
  if (service_id != SERVICE_ID) {
    return;
  }

  if (available) {
    text_layer_set_background_color(status_text_layer, GColorGreen);
    text_layer_set_text(status_text_layer, "Docked");
  } else {
    text_layer_set_background_color(status_text_layer, GColorYellow);
    text_layer_set_text(status_text_layer, "Undocked");
  }
}

static void prv_did_read(SmartstrapAttribute *attr, SmartstrapResult result,
                         const uint8_t *data, size_t length) {
  if (attr != uptime_attribute) {
    return;
  }
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read failed with result %d", result);
    return;
  }
  if (length != UPTIME_ATTRIBUTE_LENGTH) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Got response of unexpected length (%d)", length);
    return;
  }

  static char uptime_buffer[20];
  snprintf(uptime_buffer, 20, "%u", (unsigned int)*(uint32_t *)data);
  //text_layer_set_text(uptime_text_layer, uptime_buffer);
}

static void PrvSayTime() {
  SmartstrapResult result;
  uint8_t *bufferPtr;
  
  size_t length;


  result = smartstrap_attribute_begin_write(sayTime_attribute, &bufferPtr, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  bufferPtr[0] = (uint8_t) tick_time->tm_hour;
  bufferPtr[1] = (uint8_t) tick_time->tm_min;
 
  uint16_t debugVal = bufferPtr[0];
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Hour: %u", debugVal);
  debugVal = bufferPtr[1];
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Minute: %u", debugVal);

  result = smartstrap_attribute_end_write(sayTime_attribute, 2, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}

static void prv_notified(SmartstrapAttribute *attribute) {
  if (attribute == ultrasoundAttribute) {
  	  PrvSayTime();
  }
  if (attribute == uptime_attribute) {
    smartstrap_attribute_read(uptime_attribute);
  }
}


static void PrvMakeSound() {
  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(led_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = 1;

  result = smartstrap_attribute_end_write(led_attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}


static void PrvReNotify(bool startNotify) {
  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(renotifyAttribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = startNotify;

  result = smartstrap_attribute_end_write(renotifyAttribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  PrvMakeSound();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  PrvSayTime();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  PrvSayTime();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void focus_handler(bool inFocus) {
  // Emit a log entry to show new focus state
  if (inFocus) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Now in focus");
    PrvReNotify(true);// notify started
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Now NOT in focus");
    PrvReNotify(false);// notify started
  }
}


static void update_time() {
 // Get a tm structure
 time_t temp = time(NULL);
 struct tm *tick_time = localtime(&temp);  // Create a long-lived buffer
 static char buffer[] = "00:00";  // Write the current hours and minutes into the buffer
 if(clock_is_24h_style() == true) {
   // Use 24 hour format
   strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
 } else {
   // Use 12 hour format
   strftime(buffer, sizeof("00:00"), "%l:%M", tick_time);
 }  // Display this time on the TextLayer
 text_layer_set_text(uptime_text_layer, buffer);
} 


static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
 update_time();
}


static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);

  // text layer for connection status
  status_text_layer = text_layer_create(GRect(0, 0, 144, 40));
  text_layer_set_font(status_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(status_text_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(status_text_layer));
  prv_availability_changed(SERVICE_ID, smartstrap_service_is_available(SERVICE_ID));

  // text layer for showing the attribute
//  uptime_text_layer = text_layer_create(GRect(0, 60, 144, 40));
  uptime_text_layer = text_layer_create(GRect(0, 55, 144, 50));
 // text_layer_set_font(uptime_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
 // text_layer_set_text_alignment(uptime_text_layer, GTextAlignmentCenter);
	text_layer_set_font(uptime_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(uptime_text_layer, GTextAlignmentCenter); 
  text_layer_set_text(uptime_text_layer, "-");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(uptime_text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(status_text_layer);
  text_layer_destroy(uptime_text_layer);
}

static void init(void) {
  // setup window
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  // setup smartstrap
  SmartstrapHandlers handlers = (SmartstrapHandlers) {
    .availability_did_change = prv_availability_changed,
    .did_read = prv_did_read,
    .notified = prv_notified
  };
  smartstrap_subscribe(handlers);
  led_attribute = smartstrap_attribute_create(SERVICE_ID, SOUND_ATTRIBUTE_ID, SOUND_ATTRIBUTE_LENGTH);
  uptime_attribute = smartstrap_attribute_create(SERVICE_ID, UPTIME_ATTRIBUTE_ID,
                                                 UPTIME_ATTRIBUTE_LENGTH);
  sayTime_attribute = smartstrap_attribute_create(SERVICE_ID, SayTimeAttributeID,
                                                 SayTimeAttributeIDLength);
  renotifyAttribute = smartstrap_attribute_create(SERVICE_ID, RenotifyAttributeId,
                                                 RenotifyAttributeIdLength);
  ultrasoundAttribute = smartstrap_attribute_create(SERVICE_ID, ultrasoundAttributeID,
                                                 ultrasoundAttributeIdLength);
 
  // Subscribe to Focus updates
  app_focus_service_subscribe(focus_handler);
 // Register with TickTimerService
 tick_timer_service_subscribe(MINUTE_UNIT, tick_handler); 
}

static void deinit(void) {
  window_destroy(window);
  smartstrap_attribute_destroy(led_attribute);
  smartstrap_attribute_destroy(uptime_attribute);
  smartstrap_attribute_destroy(sayTime_attribute);
  smartstrap_attribute_destroy(renotifyAttribute);
  smartstrap_attribute_destroy(ultrasoundAttribute);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
