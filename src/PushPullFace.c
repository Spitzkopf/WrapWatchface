#include <pebble.h>
#include "LayerCollection.h"

#define PUSH_PULL_DURATION 300
#define PUSH_PULL_DELAY 200

#define PUSH_PULL_DURATION_SEC 250
#define PUSH_PULL_DELAY_SEC 0

#define MAX_W 144
#define MAX_H 168

typedef void(*PushPullAnimationEndCallback)(void*);

typedef enum {
  Self = 0,
  Other = 1
} WrapTarget;

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
  WrapTarget wrap;
  
  PushPullAnimationEndCallback callback;
  void* callback_data;
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

TextLayer* s_time_1;
TextLayer* s_time_2;

TextLayer* s_date_1;
TextLayer* s_date_2;

TextLayer* s_bt_1;
TextLayer* s_bt_2;

TextLayer* s_seconds_1;
TextLayer* s_seconds_2;

TextLayer* s_battery_1;
TextLayer* s_battery_2;

void* time_row;
void* date_row;
void* bt_row;
void* seconds_row;
void* battery_row;

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

void generic_animation_stop_handler(Animation *animation, bool finished, void *context) {
  property_animation_destroy((PropertyAnimation*)animation);
  PushPullAnimationData* data = (PushPullAnimationData*)context;
  
  if (data && data->callback) {
    data->callback(data->callback_data);
  }
}

void wrap_around_on_stop_handler(Animation *animation, bool finished, void *context) {
  PushPullAnimationData* data = (PushPullAnimationData*)context;
  property_animation_destroy((PropertyAnimation*)animation);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Animation Status %d", finished);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrap Status %d", data->wrap);
  
  if (finished && data && Self == data->wrap) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done, Wrapping!");
    layer_set_frame(data->this_layer, data->wrap_location);
    GRect current_location = layer_get_frame(data->this_layer);
    PropertyAnimation* anim = prepare_animation(data->this_layer, &current_location, &data->next_layer_start, data->duration, 0);
    add_animation_handlers(anim, NULL, generic_animation_stop_handler, NULL);
    schedule_animation(anim);
    free(data);
  }
}

void wrap_around_on_start_handler(Animation *animation, void *context) {
  PushPullAnimationData* data = (PushPullAnimationData*)context;
  PushPullAnimationData* new_data = malloc(sizeof(PushPullAnimationData));
  PropertyAnimation* anim;
  
  new_data->wrap = Self;
  new_data->duration = data->duration;
  new_data->this_layer = data->next_layer;
  new_data->next_layer_start = data->this_start;
  new_data->wrap_location = data->wrap_location;
  
  if (Other == data->wrap) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrapping Other!");
    anim = prepare_animation(data->next_layer, &data->next_layer_start, &data->next_layer_stop, data->duration, 0);
    add_animation_handlers(anim, NULL, wrap_around_on_stop_handler, new_data);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrapping Self!");
    anim = prepare_animation(data->next_layer, &data->next_layer_start, &data->next_layer_stop, data->duration, 0);
    add_animation_handlers(anim, NULL, generic_animation_stop_handler, NULL);
  }
  schedule_animation(anim);
}

GRect calculate_layer_to_hide_location(GRect location, PushPullDx dx) {
  int16_t new_x;
  
  switch (dx) {
    case Left:
      new_x = (location.size.w * -1);
      return GRect(new_x, location.origin.y, location.size.w, location.size.h);
     case Right:
      return GRect(MAX_W, location.origin.y, location.size.w, location.size.h);
  }
  
  return GRectZero;
}

void push_pull_effect(Layer* showing, Layer* hidden, PushPullDx dx, int duration, int dealy, PushPullAnimationEndCallback callback, void* callback_data) {
  GRect showing_start = layer_get_frame(showing);
  GRect hidden_start = layer_get_frame(hidden);
  
  GRect showing_stop;// = calculate_layer_to_hide_location(showing_start, dx);
  
  PushPullAnimationData* push_pull = malloc(sizeof(PushPullAnimationData));
  AnimationStoppedHandler handler;
  
  switch (dx) {
    case Left:
      showing_stop = GRect(showing_start.size.w * -1, showing_start.origin.y, showing_start.size.w, showing_start.size.h);
      push_pull->next_layer_stop = showing_start;
      push_pull->wrap = Self;
      push_pull->wrap_location = GRect(MAX_W, showing_start.origin.y, showing_start.size.w, showing_start.size.h);
      handler = wrap_around_on_stop_handler;
      break;
    case Right:
      showing_stop = GRect(hidden_start.origin.x, showing_start.origin.y, showing_start.size.w, showing_start.size.h);
      push_pull->next_layer_stop = GRect(MAX_W, hidden_start.origin.y, hidden_start.size.w, hidden_start.size.h);
      push_pull->wrap = Other;
      push_pull->wrap_location = GRect(MAX_W * -1, hidden_start.origin.y, hidden_start.size.w, hidden_start.size.h);
      handler = generic_animation_stop_handler;
      break;
   default:
      return;
  }
  
  push_pull->this_layer = showing;
  push_pull->this_start = showing_start;
  push_pull->this_stop = showing_stop;
  
  push_pull->next_layer = hidden;
  push_pull->next_layer_start = hidden_start;
  push_pull->duration = duration;
  push_pull->callback = callback;
  push_pull->callback_data = callback_data;
  
  
  PropertyAnimation* anim = prepare_animation(showing, &showing_start, &showing_stop, duration, dealy);
  add_animation_handlers(anim, wrap_around_on_start_handler, handler, (void*)push_pull);
  schedule_animation(anim);
}

int get_visible_area(GRect rect) {
  int hidden_x = 0;
  int hidden_y = 0;
  
  int affective_x = rect.size.w;
  int affective_y = rect.size.h;
  
  if (rect.origin.x < 0) {
    hidden_x = abs(rect.origin.x);
    if (hidden_x > rect.size.w) {
      return 0;
    }
    affective_x = rect.size.w - hidden_x;
  }
  else if (rect.origin.x > MAX_W) {
    return 0;
  }
  else if (rect.origin.x + rect.size.w > MAX_W) {
    affective_x = rect.size.w - (rect.origin.x);
  }
  
  if (rect.origin.y < 0) {
    hidden_y = abs(rect.origin.y);
    if (hidden_y > rect.size.h) {
      return 0;
    }
    affective_y = rect.size.h = hidden_y;
  }
  else if (rect.origin.y > MAX_H) {
    return 0;
  }
   else if (rect.origin.y + rect.size.h > MAX_H) {
    affective_y = rect.size.h - (rect.origin.y);
  }
  
  return affective_x * affective_y;
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

void swap_row(void* row, PushPullDx direction, int duration, int delay, PushPullAnimationEndCallback callback, void* callback_data) {
  Layer* current = get_current_layer(row);
  Layer* next = get_next_layer(row);
  push_pull_effect(current, next, direction, duration, delay, callback, callback_data);
}

void update_time_layer_1() {
  time_t temp = time(NULL); 
  
  static char buffer[] = "00:00";
  
  struct tm *tick_time = localtime(&temp);
  if(clock_is_24h_style() == true) {
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  text_layer_set_text(s_time_1, buffer);
}

void update_time_layer_2() {
  time_t temp = time(NULL); 
  
  static char buffer[] = "00:00";
  
  struct tm *tick_time = localtime(&temp);
  if(clock_is_24h_style() == true) {
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  text_layer_set_text(s_time_2, buffer);
}

static void update_date(TextLayer* layer) {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  static char buffer[] = "00/00/00";
  strftime(buffer, sizeof(buffer), "%d/%m/%y", tick_time);
  
  text_layer_set_text(layer, buffer);
}

static void update_seconds_layer_1() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  static char buffer[] = "00";
  strftime(buffer, sizeof(buffer), "%S", tick_time);
  
  text_layer_set_text(s_seconds_1, buffer);
}

static void update_seconds_layer_2() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  static char buffer[] = "00";
  strftime(buffer, sizeof(buffer), "%S", tick_time);
  
  text_layer_set_text(s_seconds_2, buffer);
}

void update_other_seconds(void* update_proc) {
  typedef void (*seconds_update_proc)(void);
  seconds_update_proc proc = (seconds_update_proc)update_proc;
  proc();
}

void update_other_minutes(void* update_proc) {
  typedef void (*minutes_update_proc)(void);
  minutes_update_proc proc = (minutes_update_proc)update_proc;
  proc();
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  Layer* current;
  
  if((units_changed & MINUTE_UNIT) != 0) {
    current = get_current_layer(time_row);
    if (0 == current_layer_index(time_row)) {
      update_time_layer_2();
      swap_row(time_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_other_minutes, update_time_layer_1);
    } else {
      update_time_layer_1();
      swap_row(time_row, Left, PUSH_PULL_DURATION, PUSH_PULL_DELAY, update_other_minutes, update_time_layer_2);
    }
  }

  if((units_changed & DAY_UNIT) != 0) {
    swap_row(date_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL, NULL);
    current = get_current_layer(date_row);
    update_date((TextLayer*)current);
  }
  
  if((units_changed & SECOND_UNIT) != 0) {
    current = get_current_layer(seconds_row);
    if (0 == current_layer_index(seconds_row)) {
      update_seconds_layer_2();
      swap_row(seconds_row, Left, PUSH_PULL_DURATION_SEC, PUSH_PULL_DELAY_SEC, update_other_seconds, update_seconds_layer_1);
    } else {
      update_seconds_layer_1();
      swap_row(seconds_row, Left, PUSH_PULL_DURATION_SEC, PUSH_PULL_DELAY_SEC, update_other_seconds, update_seconds_layer_2);
    }
  }
}

void bluetooth_state_changed(bool connected) {
  if (connected) {
    if (0 != current_layer_index(bt_row)) {
      swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL, NULL);
    }
  } else if (1 != current_layer_index(bt_row)) {
    swap_row(bt_row, Right, PUSH_PULL_DURATION, PUSH_PULL_DELAY, NULL, NULL);
  }
}

static void window_load(Window *window) {
  s_time_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_time_1, GColorBlack);
  text_layer_set_text_color(s_time_1, GColorWhite);
  text_layer_set_font(s_time_1, custom_font_28);
  text_layer_set_text_alignment(s_time_1, GTextAlignmentCenter);
  update_time_layer_1();
  
  s_time_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_time_2, GColorWhite);
  text_layer_set_text_color(s_time_2, GColorBlack);
  text_layer_set_font(s_time_2, custom_font_28);
  text_layer_set_text_alignment(s_time_2, GTextAlignmentCenter);
  update_time_layer_2();
  
  s_date_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_date_1, GColorWhite);
  text_layer_set_text_color(s_date_1, GColorBlack);
  text_layer_set_font(s_date_1, custom_font_28);
  text_layer_set_text_alignment(s_date_1, GTextAlignmentCenter);
  update_date(s_date_1);
  
  s_date_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_date_2, GColorBlack);
  text_layer_set_text_color(s_date_2, GColorWhite);
  text_layer_set_font(s_date_2, custom_font_28);
  text_layer_set_text_alignment(s_date_2, GTextAlignmentCenter);
  update_date(s_date_2);
  
  s_bt_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_bt_1, GColorBlack);
  text_layer_set_text_color(s_bt_1, GColorWhite);
  text_layer_set_text(s_bt_1, "Bluetooth On");
  text_layer_set_font(s_bt_1, custom_font_20);
  text_layer_set_text_alignment(s_bt_1, GTextAlignmentCenter);
  
  s_bt_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_bt_2, GColorWhite);
  text_layer_set_text_color(s_bt_2, GColorBlack);
  text_layer_set_text(s_bt_2, "Bluetooth Off"); 
  text_layer_set_font(s_bt_2, custom_font_20);
  text_layer_set_text_alignment(s_bt_2, GTextAlignmentCenter);
  
  s_seconds_1 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_seconds_1, GColorWhite);
  text_layer_set_text_color(s_seconds_1, GColorBlack);
  text_layer_set_font(s_seconds_1, custom_font_28);
  text_layer_set_text_alignment(s_seconds_1, GTextAlignmentCenter);
  update_seconds_layer_1();
  
  s_seconds_2 = text_layer_create(GRectZero);
  text_layer_set_background_color(s_seconds_2, GColorBlack);
  text_layer_set_text_color(s_seconds_2, GColorWhite);
  text_layer_set_font(s_seconds_2, custom_font_28);
  text_layer_set_text_alignment(s_seconds_2, GTextAlignmentCenter);
  update_seconds_layer_2();
  
  time_row = initialize_row(window, text_layer_get_layer(s_time_1), text_layer_get_layer(s_time_2), RSP_80, 0, 42);
  date_row = initialize_row(window, text_layer_get_layer(s_date_1), text_layer_get_layer(s_date_2), RSP_40, 42, 42);
  bt_row = initialize_row(window, text_layer_get_layer(s_bt_1), text_layer_get_layer(s_bt_2), RSP_70, 84, 42);
  seconds_row = initialize_row(window, text_layer_get_layer(s_seconds_1), text_layer_get_layer(s_seconds_2), RSP_80, 126, 42);
  
  bluetooth_state_changed(bluetooth_connection_service_peek());
}

static void window_unload(Window *window) {
  destroy_layer_collection(time_row);
  destroy_layer_collection(date_row);
  destroy_layer_collection(bt_row);
  destroy_layer_collection(seconds_row);
  
  text_layer_destroy(s_time_1);
  text_layer_destroy(s_time_2);
  text_layer_destroy(s_date_1);
  text_layer_destroy(s_date_2);
  text_layer_destroy(s_bt_1);
  text_layer_destroy(s_bt_2);
  text_layer_destroy(s_seconds_1);
  text_layer_destroy(s_seconds_2);
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
