#include <pebble.h>
#include <math.h>

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
  RSP_10 = 10,
  RSP_20 = 20,
  RSP_30 = 30,
  RSP_40 = 40,
  RSP_50 = 50,
  RSP_60 = 60,
  RSP_70 = 70,
  RSP_80 = 80,
  RSP_90 = 90,
  RSP_100 = 100,
} RowShowPercentage;

GFont custom_font_20;
GFont custom_font_24;
GFont custom_font_28;

Window* main_window;

TextLayer* minutes_1;
TextLayer* minutes_2;

TextLayer* date_1;
TextLayer* date_2;

TextLayer* bt_1;
TextLayer* bt_2;

TextLayer* seconds_1;
TextLayer* seconds_2;

void* time_row;
void* date_row;
void* bt_row;
void* seconds_row;

void* row_collection;

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

void MAKE_UPDATE_FN_NAME(minutes)(TextLayer* layer, char* buffer, int buffer_size) {
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
MAKE_UPDATE_TEXT_LAYER_PAIR(minutes, "00:00");
MAKE_UPDATE_TEXT_LAYER_PAIR(date, "00/00/00");

void swap_row(void* row, PushPullDx direction, int duration, int delay, PushPullAnimationEndCallback callback) {
  Layer* current = get_current_layer(row);
  Layer* next = get_next_layer(row);
  push_pull_effect(current, next, direction, duration, delay, callback);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if((units_changed & MINUTE_UNIT) != 0) {
    if (0 == current_layer_index(time_row)) {
      update_minutes_layer_2();
      swap_row(time_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_minutes_layer_1);
    } else {
      update_minutes_layer_1();
      swap_row(time_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_minutes_layer_2);
    }
  }

  if((units_changed & DAY_UNIT) != 0) {
    if (0 == current_layer_index(date_row)) {
      update_date_layer_2();
      swap_row(date_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_date_layer_1);
    } else {
      update_date_layer_1();
      swap_row(date_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_date_layer_2);
    }
  }
  
  if((units_changed & SECOND_UNIT) != 0) {
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
  if (connected) {
    if (0 != current_layer_index(bt_row)) {
      swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL);
    }
  } else if (1 != current_layer_index(bt_row)) {
    swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL);
  }
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
  minutes_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(minutes_1, GColorBlack);
  text_layer_set_text_color(minutes_1, GColorWhite);
  text_layer_set_font(minutes_1, custom_font_28);
  text_layer_set_text_alignment(minutes_1, GTextAlignmentCenter);
  update_minutes_layer_1();
  
  minutes_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(minutes_2, GColorWhite);
  text_layer_set_text_color(minutes_2, GColorBlack);
  text_layer_set_font(minutes_2, custom_font_28);
  text_layer_set_text_alignment(minutes_2, GTextAlignmentCenter);
  update_minutes_layer_2();
  
  date_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(date_1, GColorWhite);
  text_layer_set_text_color(date_1, GColorBlack);
  text_layer_set_font(date_1, custom_font_28);
  text_layer_set_text_alignment(date_1, GTextAlignmentCenter);
  update_date_layer_1();
  
  date_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(date_2, GColorBlack);
  text_layer_set_text_color(date_2, GColorWhite);
  text_layer_set_font(date_2, custom_font_28);
  text_layer_set_text_alignment(date_2, GTextAlignmentCenter);
  update_date_layer_2();
  
  bt_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(bt_1, GColorBlack);
  text_layer_set_text_color(bt_1, GColorWhite);
  text_layer_set_text(bt_1, "Bluetooth On");
  text_layer_set_font(bt_1, custom_font_20);
  text_layer_set_text_alignment(bt_1, GTextAlignmentCenter);
  
  bt_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(bt_2, GColorWhite);
  text_layer_set_text_color(bt_2, GColorBlack);
  text_layer_set_text(bt_2, "Bluetooth Off"); 
  text_layer_set_font(bt_2, custom_font_20);
  text_layer_set_text_alignment(bt_2, GTextAlignmentCenter);
  
  seconds_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(seconds_1, GColorWhite);
  text_layer_set_text_color(seconds_1, GColorBlack);
  text_layer_set_font(seconds_1, custom_font_28);
  text_layer_set_text_alignment(seconds_1, GTextAlignmentCenter);
  update_seconds_layer_1();
  
  seconds_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(seconds_2, GColorBlack);
  text_layer_set_text_color(seconds_2, GColorWhite);
  text_layer_set_font(seconds_2, custom_font_28);
  text_layer_set_text_alignment(seconds_2, GTextAlignmentCenter);
  update_seconds_layer_2();
  
  row_collection = ll_init_linked_list();
  time_row = initialize_row_n_out_of(window, text_layer_get_layer(minutes_1), text_layer_get_layer(minutes_2), RSP_60, 0, 4);
  seconds_row = initialize_row_n_out_of(window, text_layer_get_layer(seconds_1), text_layer_get_layer(seconds_2), RSP_60, 1, 4);
  date_row = initialize_row_n_out_of(window, text_layer_get_layer(date_1), text_layer_get_layer(date_2), RSP_60, 2, 4);
  bt_row = initialize_row_n_out_of(window, text_layer_get_layer(bt_1), text_layer_get_layer(bt_2), RSP_60, 3, 4);
  
  ll_add_item(row_collection, time_row);
  ll_add_item(row_collection, seconds_row);
  ll_add_item(row_collection, date_row);
  ll_add_item(row_collection, bt_row);
  
  bluetooth_state_changed(bluetooth_connection_service_peek());
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
  custom_font_20 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGI_20));
  custom_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGI_24));
  custom_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGI_28));
  
  tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT | SECOND_UNIT, tick_handler);
  bluetooth_connection_service_subscribe(bluetooth_state_changed);
  
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(main_window, animated);
}

static void deinit(void) {
  window_destroy(main_window);
  animation_unschedule_all();
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
