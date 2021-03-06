#include <pebble.h>
#include <math.h>

#include "Config.h"
#include "PushPullUtils.h"
#include "LinkedList.h"
#include "LayerCollection.h"

#define PUSH_PULL_DURATION 300
#define PUSH_PULL_DELAY 200

#define PUSH_PULL_DURATION_SEC 250
#define PUSH_PULL_DELAY_SEC 0

#define MAX_W 144
#define MAX_H 168

typedef void(*PushPullAnimationEndCallback)(void);

typedef struct {
  Layer* this_layer;
  Layer* next_layer;
  
  GRect this_start;
  GRect this_stop;
  
  GRect next_layer_start;
  GRect next_layer_stop;
  
  
  GRect wrap_location;

  int duration;
  int delay;
  
  PushPullAnimationEndCallback callback;
} PushPullAnimationData;

typedef enum {
  Left = 0,
  Right = 1
} PushPullDx;

typedef enum {
  RSP_0 = 0,
  RSP_5 = 5,
  RSP_10 = 10,
  RSP_15 = 15,
  RSP_20 = 20,
  RSP_25 = 25,
  RSP_30 = 30,
  RSP_35 = 35,
  RSP_40 = 40,
  RSP_45 = 45,
  RSP_50 = 50,
  RSP_55 = 55,
  RSP_60 = 60,
  RSP_65 = 65,
  RSP_70 = 70,
  RSP_75 = 75,
  RSP_80 = 80,
  RSP_85 = 85,
  RSP_90 = 90,
  RSP_95 = 95,
  RSP_100 = 100,
} RowShowPercentage;

GFont custom_font_20;
GFont custom_font_24;
GFont custom_font_28;

Window* main_window;

TextLayer* hours_and_minutes_1;
TextLayer* hours_and_minutes_2;

TextLayer* date_1;
TextLayer* date_2;

TextLayer* bt_1;
TextLayer* bt_2;

TextLayer* seconds_1;
TextLayer* seconds_2;

TextLayer* battery_1;
TextLayer* battery_2;

void* hours_and_minutes_row = NULL;
void* date_row = NULL;
void* bt_row = NULL;
void* seconds_row = NULL;
void* battery_row = NULL;

void* row_collection;

bool bootstrap;

PropertyAnimation* prepare_animation(Layer* layer, GRect *start, GRect *finish, int duration, int delay) {
  PropertyAnimation *anim = property_animation_create_layer_frame(layer, start, finish);
  animation_set_duration((Animation*) anim, duration);
  animation_set_delay((Animation*) anim, delay);
  return anim;
}

void add_animation_handlers(PropertyAnimation* anim, AnimationStartedHandler on_started, AnimationStoppedHandler on_stopped, void* context) {
  AnimationHandlers handlers = {
      .started = (AnimationStartedHandler) on_started,
      .stopped = (AnimationStoppedHandler) on_stopped
  };
  animation_set_handlers((Animation*) anim, handlers, context);
}

void schedule_animation(PropertyAnimation* anim) {
  animation_schedule((Animation*) anim);
}

void do_nothing_on_stop_handler(Animation *animation, bool finished, void *context) {
  PushPullAnimationEndCallback callback = (PushPullAnimationEndCallback)context;
  if (callback) {
    callback();
  }
  
  property_animation_destroy((PropertyAnimation*)animation);
}

void wrap_around_on_stop_handler(Animation *animation, bool finished, void *context) {
  PushPullAnimationData* data = (PushPullAnimationData*)context;
  property_animation_destroy((PropertyAnimation*)animation);
  
  if (finished && data) {
      layer_set_frame(data->this_layer, data->wrap_location);
      GRect current_location = layer_get_frame(data->this_layer);
      PropertyAnimation* anim = prepare_animation(data->this_layer, &current_location, &data->next_layer_start, data->duration, 0);
      add_animation_handlers(anim, NULL, do_nothing_on_stop_handler, data->callback);
      schedule_animation(anim);
      
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Freeing PushPullData %p", data);
      free(data);
  }
}

void wrap_around_on_start_handler(Animation *animation, void *context) {
  PushPullAnimationData* data = (PushPullAnimationData*)context;
  PropertyAnimation* anim;
  
  anim = prepare_animation(data->next_layer, &data->next_layer_start, &data->next_layer_stop, data->duration, 0);
  add_animation_handlers(anim, NULL, do_nothing_on_stop_handler, data->callback);
  
  schedule_animation(anim);
}

void push_pull_effect(Layer* left, Layer* right, PushPullDx dx, int duration, int dealy, PushPullAnimationEndCallback callback) {
  GRect left_start = layer_get_frame(left);
  GRect right_start = layer_get_frame(right);
  
  PushPullAnimationData* push_pull = malloc(sizeof(PushPullAnimationData));
  PropertyAnimation* anim;
  
  if (!push_pull) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error allocating push pull data");
    return;
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Allocated PushPullData %p", push_pull);
  
  switch (dx) {
    case Left:
      push_pull->this_layer = left;
      push_pull->this_start = left_start;
      push_pull->this_stop = GRect(left_start.size.w * -1, left_start.origin.y, left_start.size.w, left_start.size.h);
      push_pull->next_layer = right;
      push_pull->next_layer_start = right_start;
      push_pull->next_layer_stop = left_start;
      push_pull->wrap_location = GRect(MAX_W, left_start.origin.y, left_start.size.w, left_start.size.h);
      anim = prepare_animation(left, &left_start, &push_pull->this_stop, duration, dealy);
      break;
    case Right:
      push_pull->this_layer = right;
      push_pull->this_start = right_start;
      push_pull->this_stop = GRect(MAX_W, right_start.origin.y, right_start.size.w, right_start.size.h);
      push_pull->next_layer = left;
      push_pull->next_layer_start = left_start;
      push_pull->next_layer_stop = GRect(right_start.origin.x, left_start.origin.y, left_start.size.w, left_start.size.h);
      push_pull->wrap_location = GRect(0 - right_start.size.w, right_start.origin.y, right_start.size.w, right_start.size.h);
      anim = prepare_animation(right, &right_start, &push_pull->this_stop, duration, dealy);
      break;
   default:
      return;
  }
  
  push_pull->duration = duration;
  push_pull->callback = callback;
  
  add_animation_handlers(anim, wrap_around_on_start_handler, wrap_around_on_stop_handler, (void*)push_pull);
  schedule_animation(anim);
}

void MAKE_UPDATE_FN_NAME(seconds)(TextLayer* layer, char* buffer, int buffer_size) {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  strftime(buffer, buffer_size, "%S", tick_time);
  text_layer_set_text(layer, buffer);
}

void MAKE_UPDATE_FN_NAME(hours_and_minutes)(TextLayer* layer, char* buffer, int buffer_size) {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  if(clock_is_24h_style() == true) {
    strftime(buffer, buffer_size, "%H:%M", tick_time);
  } else {
    strftime(buffer, buffer_size, "%I:%M", tick_time);
  }

  text_layer_set_text(layer, buffer);
}

void MAKE_UPDATE_FN_NAME(date)(TextLayer* layer, char* buffer, int buffer_size) {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  strftime(buffer, buffer_size, "%d/%m/%y", tick_time);
  text_layer_set_text(layer, buffer);
}

MAKE_UPDATE_TEXT_LAYER_PAIR(seconds, "00");
MAKE_UPDATE_TEXT_LAYER_PAIR(hours_and_minutes, "00:00");
MAKE_UPDATE_TEXT_LAYER_PAIR(date, "00/00/00");

void swap_row(void* row, PushPullDx direction, int duration, int delay, PushPullAnimationEndCallback callback) {
  Layer* current = get_current_layer(row);
  Layer* next = get_next_layer(row);
  push_pull_effect(current, next, direction, duration, delay, callback);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if(((units_changed & MINUTE_UNIT) != 0) && (hours_and_minutes_row)) {
    if (0 == current_layer_index(hours_and_minutes_row)) {
      update_hours_and_minutes_layer_2();
      swap_row(hours_and_minutes_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_hours_and_minutes_layer_1);
    } else {
      update_hours_and_minutes_layer_1();
      swap_row(hours_and_minutes_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_hours_and_minutes_layer_2);
    }
  }

  if(((units_changed & DAY_UNIT) != 0) && (date_row)) {
    if (0 == current_layer_index(date_row)) {
      update_date_layer_2();
      swap_row(date_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_date_layer_1);
    } else {
      update_date_layer_1();
      swap_row(date_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_date_layer_2);
    }
  }
  
  if(((units_changed & SECOND_UNIT) != 0) && (seconds_row)) {
    if (0 == current_layer_index(seconds_row)) {
      update_seconds_layer_2();
      swap_row(seconds_row, Left, PUSH_PULL_DURATION_SEC, PUSH_PULL_DELAY_SEC, update_seconds_layer_1);
    } else {
      update_seconds_layer_1();
      swap_row(seconds_row, Left, PUSH_PULL_DURATION_SEC, PUSH_PULL_DELAY_SEC, update_seconds_layer_2);
    }
  }
}

void bluetooth_state_changed(bool connected) {
  if (!bt_row) {
    return;
  }
  
  if (connected) {
    if (0 != current_layer_index(bt_row)) {
      swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL);
    }
  } else if (1 != current_layer_index(bt_row)) {
    swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL);
  }
}

void battery_state_changed(BatteryChargeState charge) {
  if (!battery_row) {
    return;
  }
  
  PushPullDx direction = Left;
  if (charge.is_charging) {
    direction = Right;
  }
  static int previous_charge = 0;
  static char layer_1_buffer[32];
  static char layer_2_buffer[32];

  snprintf(layer_1_buffer, sizeof(layer_1_buffer), "%d%%", charge.charge_percent);
  text_layer_set_text(battery_1, layer_1_buffer);
  snprintf(layer_2_buffer, sizeof(layer_2_buffer), "%d%%", charge.charge_percent);
  text_layer_set_text(battery_2, layer_2_buffer);
  
  if (!bootstrap && previous_charge != charge.charge_percent) {
    swap_row(battery_row, direction, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL);
  }
  
  previous_charge = charge.charge_percent;
}

void* initialize_row(Window* window, Layer* first, Layer* second, RowShowPercentage show_percentage, int y, int h) {
  void* layer_pair = init_layer_collection();
  
  layer_set_frame(first, GRect((144 - (144 * (show_percentage / 100.0))) * -1, y, 144, h));
  GRect layer1_frame = layer_get_frame(first);
  
  layer_set_frame(second, GRect(layer1_frame.origin.x + layer1_frame.size.w, y, 144, h));
  
  layer_add_child(window_get_root_layer(window), first);
  layer_add_child(window_get_root_layer(window), second);
  
  add_layer(layer_pair, first);
  add_layer(layer_pair, second);
  
  return layer_pair;
}

void* initialize_row_n_out_of(Window* window, Layer* first, Layer* second, RowShowPercentage show_percentage, int index, int total)
{
  int layer_size = ceil(MAX_H / (double)total);
  int last_layer_size = MAX_H - (layer_size * (total - 1));
  
  int chosen_size = layer_size;
  
  if (index > (total - 1)) {
    return NULL;
  }
  
  if (index == (total - 1)) {
    chosen_size = last_layer_size;
  }
  
  return initialize_row(window, first, second, show_percentage, index * layer_size, chosen_size); 
}

static void window_load(Window *window) {
  bootstrap = true;
    
  MAKE_TEXT_LAYER_PAIR(seconds, custom_font_28);

  update_seconds_layer_1();
  update_seconds_layer_2();
  
  MAKE_TEXT_LAYER_PAIR(hours_and_minutes, custom_font_28);
  
  update_hours_and_minutes_layer_1();
  update_hours_and_minutes_layer_2();
  
  MAKE_TEXT_LAYER_PAIR(date, custom_font_28);
  
  update_date_layer_1();
  update_date_layer_2();
  
  MAKE_TEXT_LAYER_PAIR(bt, custom_font_20);
  
  text_layer_set_text(bt_1, "Bluetooth On");
  text_layer_set_text(bt_2, "Bluetooth Off"); 
  
  MAKE_TEXT_LAYER_PAIR(battery, custom_font_28);
  
  row_collection = ll_init_linked_list();
  
  if (!row_collection)
  {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error initializing row_collection linked list");
    return;
  }
  
  ROW_BOOTSTRAP_BEGIN(5)
    ROW_ADD_LAYER(hours_and_minutes_row, window, text_layer_get_layer(hours_and_minutes_1), text_layer_get_layer(hours_and_minutes_2), RSP_65);
    ROW_ADD_LAYER(seconds_row, window, text_layer_get_layer(seconds_1), text_layer_get_layer(seconds_2), RSP_65);
    ROW_ADD_LAYER(date_row, window, text_layer_get_layer(date_1), text_layer_get_layer(date_2), RSP_65);
    ROW_ADD_LAYER(bt_row, window, text_layer_get_layer(bt_1), text_layer_get_layer(bt_2), RSP_65);
    ROW_ADD_LAYER(battery_row, window, text_layer_get_layer(battery_1), text_layer_get_layer(battery_2), RSP_65);
  ROW_BOOTSTRAP_END
  
  ll_add_item(row_collection, hours_and_minutes_row);
  ll_add_item(row_collection, seconds_row);
  ll_add_item(row_collection, date_row);
  ll_add_item(row_collection, bt_row);
  ll_add_item(row_collection, battery_row);
  
  bluetooth_state_changed(bluetooth_connection_service_peek());
  battery_state_changed(battery_state_service_peek());
  
  bootstrap = false;
}

static void window_unload(Window *window) {
  int i = 0;
  int j = 0;
  int row_count = ll_item_count(row_collection);
  int layer_count = 0;
  for (; i != (row_count - 1); ++i) {
    void* row = ll_get_item_at(row_collection, i);
    if (row) {
      layer_count = get_layer_count(row);
      for (; j != (layer_count - 1); ++j) {
        text_layer_destroy((TextLayer*)get_layer_at(row, j));
      }
      destroy_layer_collection(row);
    }
  }
  
  ll_destroy_linked_list(row_collection);
}

static void init(void) {
  main_window = window_create(); 
  
  window_set_background_color(main_window, BACKGROUND_COLOR);
  
  custom_font_20 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RE_20));
  custom_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RE_24));
  custom_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RE_28));
  
  tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT | SECOND_UNIT, tick_handler);
  
  bluetooth_connection_service_subscribe(bluetooth_state_changed);
  battery_state_service_subscribe(battery_state_changed);
  
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(main_window, true);
}

static void deinit(void) {
  window_destroy(main_window);
  animation_unschedule_all();
  
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  
  fonts_unload_custom_font(custom_font_20);
  fonts_unload_custom_font(custom_font_24);
  fonts_unload_custom_font(custom_font_28);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", main_window);

  app_event_loop();
  deinit();
}
