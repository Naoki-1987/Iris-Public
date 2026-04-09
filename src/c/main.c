#include <pebble.h>

// --- 設定・構造体 ---
typedef enum { WeatherClear, WeatherCloudy, WeatherRainy, WeatherSnowy, WeatherThunder } WeatherStatus;

typedef struct { 
  int watchface_mode; int theme; int anim_speed; int anim_freq; int vibe_mode; int shutter_thick; 
  bool battery_color_sync; int shutter_color_mode; GColor custom_shutter_color; 
  WeatherStatus weather; WeatherStatus tomorrow_weather; 
  bool enable_business; int biz_start_h; int biz_end_h; 
  int biz_face; int biz_theme; int biz_shutter; int chime_interval;
  int biz_days; int biz_chime_interval; int biz_vibe_mode;
} ClaySettings;

static ClaySettings s_settings;
static ClaySettings s_pending_settings; 
static bool s_has_pending_settings = false; 

//ビジネスモードの状態管理
static bool is_biz_mode = false;        // 現在の「判定」結果
static bool s_active_biz_mode = false;  // 現在「表示」されているモード（シャッター裏ですり替える用）
static bool is_sleep_mode = false;      // スリープモード（強制ミュート）判定
static bool s_pending_biz_update = false;
static bool s_in_focus = true;  // 今画面がちゃんと見えているか？
//最後に時報が鳴った時間を記憶する変数！
static time_t s_last_chime_time = 0;

static Window *s_main_window;
static Layer *s_canvas_layer;
static char s_time_chars[4][2]; 
static char s_date_str[16];
static GFont s_time_font = NULL, s_info_font;

static float s_progress = 0.0; 
static bool s_is_animating = false, s_vibrate_on_close = false;
static int s_unobstructed_y_offset = 0; 
static BatteryChargeState s_last_battery;
static char s_steps_str[16] = "00.0K"; 

static int s_current_f_size = 0;

static GPathInfo s_path_info = {.num_points = 4, .points = (GPoint[4]){{0,0},{0,0},{0,0},{0,0}}};
static GPath *s_dynamic_path = NULL;

#define FRAME_RATE_MS 33

// --- プロトタイプ宣言（コンパイラエラー回避！） ---
static void adjust_layout(int16_t unobstructed_h);
static void update_info(); 
static void trigger_shutter_internal(bool manual);
static void apply_theme_colors();

// --- 設定・カラー管理 ---
static void apply_theme_colors() {
  GColor bg;
  if (s_active_biz_mode) {
    // ビジネスモード時の背景色強制上書き
    bg = (s_settings.biz_theme == 0 || s_settings.biz_theme == 1) ? GColorBlack : GColorWhite;
  } else {
    // 通常時の背景色
    bg = (s_settings.theme == 0) ? GColorWhite : GColorBlack;
  }
  window_set_background_color(s_main_window, bg);
}

static void load_settings() {
  #if defined(PBL_COLOR)
    s_settings = (ClaySettings){0, 0, 40, 1, 2, 1, true, 4, GColorCyan, WeatherCloudy, WeatherClear, false, 9, 18, 1, 0, 0, 60, 1, 60, 1};
  #else
    s_settings = (ClaySettings){0, 0, 40, 1, 2, 1, true, 0, GColorCyan, WeatherCloudy, WeatherClear, false, 9, 18, 1, 0, 0, 60, 1, 60, 1};
  #endif
  
  if(persist_exists(1)) persist_read_data(1, &s_settings, sizeof(s_settings));

  //再起動しても、自分がビジネスモードだったかを思い出す！
  if(persist_exists(2)) {
    s_active_biz_mode = persist_read_bool(2);
    is_biz_mode = s_active_biz_mode;
  }
}

static void save_settings() { persist_write_data(1, &s_settings, sizeof(s_settings)); }

static int get_dict_int(Tuple *t) {
  if (!t) return 0;
  if (t->type == TUPLE_CSTRING) return atoi(t->value->cstring);
  return t->value->int32;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *t;
	
  // =========================================================================
  // スマホから「設定教えて！」とリクエストが来た時、全データを送り返す処理
  // =========================================================================
  if (dict_find(iter, MESSAGE_KEY_RequestSettings)) {
    DictionaryIterator *out_iter;
    app_message_outbox_begin(&out_iter);
    if (out_iter != NULL) {
      //時計が持っている設定をすべて辞書に詰め込む！
      dict_write_int32(out_iter, MESSAGE_KEY_WatchFaceMode, s_settings.watchface_mode);
      dict_write_int32(out_iter, MESSAGE_KEY_Theme, s_settings.theme);
      dict_write_int32(out_iter, MESSAGE_KEY_AnimSpeed, s_settings.anim_speed);
      dict_write_int32(out_iter, MESSAGE_KEY_AnimFreq, s_settings.anim_freq);
      dict_write_int32(out_iter, MESSAGE_KEY_VibeMode, s_settings.vibe_mode);
      dict_write_int32(out_iter, MESSAGE_KEY_ShutterThickness, s_settings.shutter_thick);
      dict_write_int32(out_iter, MESSAGE_KEY_BatteryColorSync, s_settings.battery_color_sync ? 1 : 0);
      dict_write_int32(out_iter, MESSAGE_KEY_ShutterColorMode, s_settings.shutter_color_mode);
      
      //色(GColor)は特殊！スマホで読めるように24bitのHEX(16進数)カラーコードに戻してあげる
      int r8 = s_settings.custom_shutter_color.r * 85;
      int g8 = s_settings.custom_shutter_color.g * 85;
      int b8 = s_settings.custom_shutter_color.b * 85;
      dict_write_int32(out_iter, MESSAGE_KEY_CustomColor, (r8 << 16) | (g8 << 8) | b8);

      dict_write_int32(out_iter, MESSAGE_KEY_EnableBusiness, s_settings.enable_business ? 1 : 0);
      dict_write_int32(out_iter, MESSAGE_KEY_BizStartH, s_settings.biz_start_h);
      dict_write_int32(out_iter, MESSAGE_KEY_BizEndH, s_settings.biz_end_h);
      dict_write_int32(out_iter, MESSAGE_KEY_BizFace, s_settings.biz_face);
      dict_write_int32(out_iter, MESSAGE_KEY_BizTheme, s_settings.biz_theme);
      dict_write_int32(out_iter, MESSAGE_KEY_BizShutter, s_settings.biz_shutter);
      dict_write_int32(out_iter, MESSAGE_KEY_ChimeInterval, s_settings.chime_interval);
      dict_write_int32(out_iter, MESSAGE_KEY_BizDays, s_settings.biz_days);
      dict_write_int32(out_iter, MESSAGE_KEY_BizChimeInterval, s_settings.biz_chime_interval);
      dict_write_int32(out_iter, MESSAGE_KEY_BizVibeMode, s_settings.biz_vibe_mode);

      app_message_outbox_send();
    }
    return;
  }
  // =========================================================================
	
  s_pending_settings = s_settings;
  s_has_pending_settings = true; 

  if((t=dict_find(iter, MESSAGE_KEY_WatchFaceMode))) s_pending_settings.watchface_mode = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_Theme))) s_pending_settings.theme = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_AnimSpeed))) s_pending_settings.anim_speed = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_AnimFreq))) s_pending_settings.anim_freq = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_VibeMode))) s_pending_settings.vibe_mode = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_ShutterThickness))) s_pending_settings.shutter_thick = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BatteryColorSync))) s_pending_settings.battery_color_sync = get_dict_int(t) == 1;
  if((t=dict_find(iter, MESSAGE_KEY_ShutterColorMode))) s_pending_settings.shutter_color_mode = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_CustomColor))) s_pending_settings.custom_shutter_color = GColorFromHEX(get_dict_int(t));
  if((t=dict_find(iter, MESSAGE_KEY_WeatherStatus))) s_pending_settings.weather = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_WeatherStatusTomorrow))) s_pending_settings.tomorrow_weather = get_dict_int(t);

  //ビジネスモード設定の受信
  if((t=dict_find(iter, MESSAGE_KEY_EnableBusiness))) s_pending_settings.enable_business = get_dict_int(t) == 1;
  if((t=dict_find(iter, MESSAGE_KEY_BizStartH))) s_pending_settings.biz_start_h = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizEndH))) s_pending_settings.biz_end_h = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizFace))) s_pending_settings.biz_face = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizTheme))) s_pending_settings.biz_theme = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizShutter))) s_pending_settings.biz_shutter = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_ChimeInterval))) s_pending_settings.chime_interval = get_dict_int(t);
	//追加した3つの設定を受信する！
  if((t=dict_find(iter, MESSAGE_KEY_BizDays))) s_pending_settings.biz_days = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizChimeInterval))) s_pending_settings.biz_chime_interval = get_dict_int(t);
  if((t=dict_find(iter, MESSAGE_KEY_BizVibeMode))) s_pending_settings.biz_vibe_mode = get_dict_int(t);
  trigger_shutter_internal(false);
}

//究極の「空気を読む」コンテキスト判定ロジック（夜勤・24時間完全対応版）
static void evaluate_context_modes() {
  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);
  
  bool new_biz = false;
  bool new_sleep = false;
  
  if (s_settings.enable_business && quiet_time_is_active()) {
    int h = t->tm_hour;
    int s = s_settings.biz_start_h;
    int e = s_settings.biz_end_h;
    
    //24時間対応＆日またぎ考慮の判定
    bool in_hours = false;
    if (s == e) {
      in_hours = true; // 開始と終了が同じなら24時間いつでも有効！
    } else {
      in_hours = (s < e) ? (h >= s && h < e) : (h >= s || h < e);
    }
    
    if (in_hours) new_biz = true;  // ビジネスモード
    else new_sleep = true;         // スリープモード
    
    // 「夜勤の土曜早朝」を金曜扱いにするロジック！
    int logical_wday = t->tm_wday; // 一旦、今日のカレンダー上の曜日を入れる

    // もし「日またぎ設定(s > e)」で、今の時間が「終了時間より前(h < e)」なら…
    if (s > e && h < e) {
      // タイムマシン発動！曜日を「1日前」にずらす！（例：土曜(6)なら金曜(5)として扱う）
      logical_wday = (logical_wday + 6) % 7; 
    }

    // 平日設定(1)で、論理的な曜日が日曜(0)か土曜(6)なら、やっぱり休日のスリープモードにする
    if (new_biz && s_settings.biz_days == 1 && (logical_wday == 0 || logical_wday == 6)) {
      new_biz = false;
      new_sleep = true;
    }
  }
  
  // モードが変わった瞬間を検知して、マジック変形のフラグを立てる！
  if (new_biz != s_active_biz_mode && !s_pending_biz_update) {
    is_biz_mode = new_biz;
    s_pending_biz_update = true;
  }
  
  is_sleep_mode = new_sleep;
}

static GPoint get_inner_vertex(int i, int r, int32_t angle, GPoint center) {
  int32_t theta = (i * TRIG_MAX_ANGLE / 12) + angle;
  return (GPoint){center.x + (r * cos_lookup(theta) / TRIG_MAX_RATIO), center.y + (r * sin_lookup(theta) / TRIG_MAX_RATIO)};
}

static GPoint get_ray_far_point(int i, int r, int32_t angle, GPoint center) {
  GPoint v = get_inner_vertex(i, r, angle, center);
  int32_t ray_angle = ((i * TRIG_MAX_ANGLE / 12) + angle) + (5 * TRIG_MAX_ANGLE / 24); 
  return (GPoint){v.x + (400 * cos_lookup(ray_angle) / TRIG_MAX_RATIO), v.y + (400 * sin_lookup(ray_angle) / TRIG_MAX_RATIO)};
}

static void next_frame_handler(void *data) {
  if (!s_is_animating) return;
  float old = s_progress; 
  float total_duration_ms = (float)s_settings.anim_speed * 10.0f;
  if (total_duration_ms < 100.0f) total_duration_ms = 100.0f; 
  float frames_needed = total_duration_ms / (float)FRAME_RATE_MS;
  float step = 2.0f / frames_needed; 
  if (step < 0.05f) step = 0.05f; 
  s_progress += step; 
  
  if (old < 1.0f && s_progress >= 1.0f) {
    update_info();
    
    // 設定すり替え
    if (s_has_pending_settings) {
      s_settings = s_pending_settings;   
      s_has_pending_settings = false;    
      save_settings();                   
      apply_theme_colors();              
      GRect unob = layer_get_unobstructed_bounds(window_get_root_layer(s_main_window));
      adjust_layout(unob.size.h);        
    }

    //ビジネスモード切り替え時のマジック変形！
    if (s_pending_biz_update) {
      s_active_biz_mode = is_biz_mode;
      s_pending_biz_update = false;
      
      //変形完了！今のモードを脳裏に焼き付ける！
      persist_write_bool(2, s_active_biz_mode); 
      
      apply_theme_colors();              
      GRect unob = layer_get_unobstructed_bounds(window_get_root_layer(s_main_window));
      adjust_layout(unob.size.h); 
    }
  }
  
  if (old < 0.9f && s_progress >= 0.9f && s_vibrate_on_close) vibes_short_pulse(); 
  if (s_progress >= 2.0f) { s_progress = 0.0f; s_is_animating = false; }
  else { app_timer_register(FRAME_RATE_MS, next_frame_handler, NULL); }
  layer_mark_dirty(s_canvas_layer);
}

static void trigger_shutter_internal(bool manual) {
  if (s_is_animating) return;
  s_is_animating = true; s_progress = 0.0;
  
  //手首シェイク時のバイブ制御（スリープモードなら絶対鳴らさない！）
  if (manual && !is_sleep_mode && (s_settings.vibe_mode == 0 || s_settings.vibe_mode == 2)) {
    s_vibrate_on_close = true; // 0: Shake&Chime, 2: Shake Only
  } else {
    s_vibrate_on_close = false;
  }
  
  app_timer_register(FRAME_RATE_MS, next_frame_handler, NULL);
}

static void handle_tap(AccelAxisType axis, int32_t direction) {
  //ビジネスモード中はシェイクを完全に無視！真面目な時計に徹する！
  if (s_active_biz_mode) return;
  
  trigger_shutter_internal(true);
}

static void handle_battery(BatteryChargeState state) { s_last_battery = state; layer_mark_dirty(s_canvas_layer); }

static void update_info() {
  time_t temp = time(NULL); struct tm *tick = localtime(&temp);
  static char s_h[4], s_m[4], s_d[16];
  
  bool h24 = clock_is_24h_style();
  strftime(s_h, sizeof(s_h), h24 ? "%H" : "%I", tick); 
  
  strftime(s_m, sizeof(s_m), "%M", tick); 
  if (s_h[0] == ' ') s_h[0] = '0';
  if (s_m[0] == ' ') s_m[0] = '0';
  
  s_time_chars[0][0] = s_h[0]; s_time_chars[0][1] = '\0';
  s_time_chars[1][0] = s_h[1]; s_time_chars[1][1] = '\0';
  s_time_chars[2][0] = s_m[0]; s_time_chars[2][1] = '\0';
  s_time_chars[3][0] = s_m[1]; s_time_chars[3][1] = '\0';

  strftime(s_d, sizeof(s_d), "%a. %b %d", tick);
  for (int i=0; s_d[i]; i++) if(s_d[i]>='a' && s_d[i]<='z') s_d[i] -= 32;
  strncpy(s_date_str, s_d, sizeof(s_date_str));

  int steps = (int)health_service_sum_today(HealthMetricStepCount);
  snprintf(s_steps_str, sizeof(s_steps_str), "%02d.%dK", steps / 1000, (steps % 1000) / 100);
  
  layer_mark_dirty(s_canvas_layer); 
}

static void handle_tick(struct tm *tick, TimeUnits units) {

  evaluate_context_modes();

  //ビジネスモードかどうかで「時報間隔」と「バイブ設定」を自動で切り替える！
  int current_interval = s_active_biz_mode ? s_settings.biz_chime_interval : s_settings.chime_interval;
  int current_vibe_mode = s_active_biz_mode ? s_settings.biz_vibe_mode : s_settings.vibe_mode;

  bool is_chime_time = false;
  if (current_interval == 60) {
    is_chime_time = (tick->tm_min == 0);
  } else if (current_interval > 0) {
    is_chime_time = (tick->tm_min % current_interval == 0);
  }
  
  //スリープモードじゃなくて、バイブ設定がONなら鳴らす！
if (is_chime_time && !is_sleep_mode) {
    //前回鳴らしてから50秒以上経過している時だけ許可！
    if (time(NULL) - s_last_chime_time > 50) {
      if (current_vibe_mode == 0 || current_vibe_mode == 1) {
        vibes_short_pulse();
        s_last_chime_time = time(NULL); // 💡 ここで時間をメモ！
      }
    }
  }

  //アニメーション制御
  if (s_pending_biz_update) {
    if (s_in_focus) trigger_shutter_internal(false); //見えてる時だけ！
    else update_info(); // 見えない時は裏で時間だけ進めて「おあずけ」！
  } else if (!s_active_biz_mode && s_settings.anim_freq > 0 && tick->tm_min % s_settings.anim_freq == 0) {
    if (s_in_focus) trigger_shutter_internal(false); //見えてる時だけ！
    else update_info(); 
  } else {
    update_info(); 
  }

  if (tick->tm_min == 0) {
    DictionaryIterator *iter; app_message_outbox_begin(&iter);
    if (iter != NULL) { dict_write_uint8(iter, 0, 1); app_message_outbox_send(); }
  }
}

static void adjust_layout(int16_t unobstructed_h) {
  GRect fb = layer_get_bounds(window_get_root_layer(s_main_window));
  s_unobstructed_y_offset = (fb.size.h - unobstructed_h) / 2;

  //ビジネスモードならシャッター太さを上書き！
  int current_shutter = s_active_biz_mode ? s_settings.biz_shutter : s_settings.shutter_thick;
  int f_size = 70;
  
  #if defined(PBL_PLATFORM_GABBRO)
    f_size = (current_shutter == 0) ? 100 : ((current_shutter == 1) ? 80 : 64);
  #elif defined(PBL_PLATFORM_EMERY)
    f_size = (current_shutter == 0) ? 100 : ((current_shutter == 1) ? 85 : 70);
  #elif defined(PBL_PLATFORM_CHALK)
    f_size = (current_shutter == 0) ? 70 : ((current_shutter == 1) ? 58 : 48);
  #else
    f_size = (current_shutter == 0) ? 70 : ((current_shutter == 1) ? 62 : 48);
  #endif

  if (s_current_f_size != f_size) {
    if (s_time_font) fonts_unload_custom_font(s_time_font);
    uint32_t res_id;

    #if defined(PBL_PLATFORM_GABBRO)
      switch (f_size) {
        case 100: res_id = RESOURCE_ID_FONT_TEKO_BOLD_100; break;
        case 80:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_80; break;
        case 64:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_64; break;
        default:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_80; break;
      }
    #elif defined(PBL_PLATFORM_EMERY)
      switch (f_size) {
        case 100: res_id = RESOURCE_ID_FONT_TEKO_BOLD_100; break;
        case 85:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_85; break;
        case 70:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_70; break;
        default:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_85; break;
      }
    #elif defined(PBL_PLATFORM_CHALK)
      switch (f_size) {
        case 70:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_70; break;
        case 58:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_58; break;
        case 48:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_48; break;
        default:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_58; break;
      }
    #else
      switch (f_size) {
        case 70:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_70; break;
        case 62:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_62; break;
        case 48:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_48; break;
        default:  res_id = RESOURCE_ID_FONT_TEKO_BOLD_62; break;
      }
    #endif

    s_time_font = fonts_load_custom_font(resource_get_handle(res_id));
    s_current_f_size = f_size; 
  }
}

static void unobstructed_did_change(void *context) {
  GRect unob = layer_get_unobstructed_bounds(window_get_root_layer(s_main_window));
  adjust_layout(unob.size.h); layer_mark_dirty(s_canvas_layer);
}

static GColor get_wing_fill_color(int wing_index) {
  // 🏢 ビジネスモード用の羽の色強制上書き
  if (s_active_biz_mode) {
    switch (s_settings.biz_theme) {
      case 0: return GColorBlack; // 黒背景 / 黒羽
      case 1: return GColorWhite; // 黒背景 / 白羽 (全部真っ白！)
      case 2: return GColorWhite; // 白背景 / 白羽
      case 3: return GColorBlack; // 白背景 / 黒羽 (全部真っ黒！)
    }
  }

  int pct = s_last_battery.charge_percent;
  int seq = (8 - wing_index + 12) % 12; 
  bool is_empty = false;
  
  if (s_settings.shutter_color_mode != 5 && s_settings.shutter_color_mode != 6 && s_settings.battery_color_sync) {
    int full_blades = (pct >= 50) ? 12 : (pct * 12) / 50;
    if (seq < (12 - full_blades)) is_empty = true;
  }

  if (is_empty) return (s_settings.theme == 0) ? GColorLightGray : GColorDarkGray;

  switch (s_settings.shutter_color_mode) {
    case 0: return GColorBlack;
    case 1: return GColorWhite;
    case 2: return s_settings.custom_shutter_color;
    case 3: { 
      static const GColor rainbow[12] = {GColorYellow, GColorIcterine, GColorKellyGreen, GColorIslamicGreen, GColorCyan, GColorBlue, GColorDukeBlue, GColorPurple, GColorMagenta, GColorRed, GColorOrange, GColorChromeYellow};
      return rainbow[wing_index];
    }
    case 4: { 
      static const GColor pastel[12] = {GColorPastelYellow, GColorRajah, GColorInchworm, GColorMintGreen, GColorCeleste, GColorBabyBlueEyes, GColorElectricBlue, GColorLavenderIndigo, GColorRichBrilliantLavender, GColorBrilliantRose, GColorMelon, GColorChromeYellow};
      return pastel[wing_index];
    }
    case 5: 
      if (pct > 80) return GColorIslamicGreen; else if (pct > 50) return GColorKellyGreen;
      else if (pct > 25) return GColorYellow; else if (pct > 5) return GColorOrange; else return GColorRed;
    case 6: 
      if (pct > 80) return GColorMintGreen; else if (pct > 50) return GColorInchworm;
      else if (pct > 25) return GColorPastelYellow; else if (pct > 5) return GColorMelon; else return GColorBrilliantRose;
    default:
      return GColorBlack; 
  }
}

static void draw_weather_icon(GContext *ctx, GPoint center, GColor color, WeatherStatus status) {
  graphics_context_set_fill_color(ctx, color); graphics_context_set_stroke_color(ctx, color);
  switch (status) {
    case WeatherClear:
      graphics_fill_circle(ctx, center, 3);
      for (int i = 0; i < 8; i++) {
        int32_t a = i * TRIG_MAX_ANGLE / 8;
        graphics_draw_line(ctx, (GPoint){center.x + (5*cos_lookup(a)/TRIG_MAX_RATIO), center.y + (5*sin_lookup(a)/TRIG_MAX_RATIO)}, (GPoint){center.x + (8*cos_lookup(a)/TRIG_MAX_RATIO), center.y + (8*sin_lookup(a)/TRIG_MAX_RATIO)});
      }
      break;
    case WeatherCloudy:
      graphics_fill_circle(ctx, (GPoint){center.x - 3, center.y + 1}, 3); graphics_fill_circle(ctx, (GPoint){center.x, center.y - 2}, 4); graphics_fill_circle(ctx, (GPoint){center.x + 3, center.y + 1}, 3);
      break;
    case WeatherRainy:
      graphics_fill_circle(ctx, (GPoint){center.x - 3, center.y + 1}, 3); graphics_fill_circle(ctx, (GPoint){center.x, center.y - 2}, 4); graphics_fill_circle(ctx, (GPoint){center.x + 3, center.y + 1}, 3);
      for (int i = 0; i < 3; i++) graphics_draw_line(ctx, (GPoint){center.x - 4 + i * 4, center.y + 3}, (GPoint){center.x - 2 + i * 4, center.y + 7});
      break;
    case WeatherSnowy:
      graphics_fill_circle(ctx, (GPoint){center.x, center.y + 2}, 3); graphics_fill_circle(ctx, (GPoint){center.x, center.y - 2}, 2); 
      break;
    case WeatherThunder:
      graphics_draw_line(ctx, (GPoint){center.x + 2, center.y - 4}, (GPoint){center.x - 2, center.y}); graphics_draw_line(ctx, (GPoint){center.x - 2, center.y}, (GPoint){center.x + 2, center.y}); graphics_draw_line(ctx, (GPoint){center.x + 2, center.y}, (GPoint){center.x - 2, center.y + 4});
      break;
  }
}

// 描画エンジン
static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer); GPoint c = grect_center_point(&b); c.y -= s_unobstructed_y_offset;
  float ev = (s_progress > 1.0) ? (2.0 - s_progress) : s_progress;
  
  //ビジネスモードなら設定を上書きする
  int current_mode = s_active_biz_mode ? s_settings.biz_face : s_settings.watchface_mode;
  int current_shutter = s_active_biz_mode ? s_settings.biz_shutter : s_settings.shutter_thick;

  int max_r = b.size.w / 2, base_r;
  if (PBL_IF_ROUND_ELSE(true, false)) {
    int wide_r = max_r - (max_r / 9); 
    int step = max_r / 7;
    if (current_shutter == 0) base_r = (int)(wide_r * 0.9659f);
    else if (current_shutter == 1) base_r = (int)((wide_r - step) * 0.9659f);
    else base_r = (int)((wide_r - step * 2) * 0.9659f);
  } else {
    #if defined(PBL_PLATFORM_EMERY)
      if (current_shutter == 0) base_r = 110;
      else if (current_shutter == 1) base_r = 94;
      else base_r = 76;
    #else
      if (current_shutter == 0) base_r = 81;
      else if (current_shutter == 1) base_r = 68;
      else base_r = 55;
    #endif
  }

  int r = (int)(base_r / 0.9659f * (1.0f - ev)); 
  int32_t angle = (int32_t)(ev * TRIG_MAX_ANGLE / 8); 

  graphics_context_set_antialiased(ctx, !s_is_animating);

//文字色・針色を「背景と同化しない高コントラスト」に設定する
  GColor txt_color;
  if (s_active_biz_mode) {
    // テーマ0,1 は黒背景 - だから文字は「白」にする！
    // テーマ2,3 は白背景 - だから文字は「黒」にする！
    txt_color = (s_settings.biz_theme <= 1) ? GColorWhite : GColorBlack;
  } else {
    txt_color = (s_settings.theme == 0 || s_settings.theme == 2) ? GColorBlack : GColorWhite;
  }

  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);
  int32_t min_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  int32_t hour_angle = TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 60) + t->tm_min) / 720;


  //アナログの針
  if (current_mode == 1 && r > 40) {
    
    int safe_r = 36; 
    int h_orbit_r = safe_r + (r - safe_r) / 2;
    
    // 破線レーダー
    graphics_context_set_stroke_color(ctx, txt_color);
    graphics_context_set_stroke_width(ctx, 1); 
    int num_dashes = 36;
    for (int i = 0; i < num_dashes; i++) {
      int32_t a1 = TRIG_MAX_ANGLE * i / num_dashes;
      int32_t a2 = TRIG_MAX_ANGLE * i / num_dashes + (TRIG_MAX_ANGLE / (num_dashes * 2));
      GPoint p1 = { c.x + (h_orbit_r * sin_lookup(a1)) / TRIG_MAX_RATIO, c.y - (h_orbit_r * cos_lookup(a1)) / TRIG_MAX_RATIO };
      GPoint p2 = { c.x + (h_orbit_r * sin_lookup(a2)) / TRIG_MAX_RATIO, c.y - (h_orbit_r * cos_lookup(a2)) / TRIG_MAX_RATIO };
      graphics_draw_line(ctx, p1, p2);
    }
    
    // 分マーカー（長針）
    graphics_context_set_stroke_color(ctx, txt_color);
    graphics_context_set_stroke_width(ctx, 5); 
    int m_d_start = h_orbit_r + 1;
    int m_d_end = b.size.w; 
    int32_t sin_m = sin_lookup(min_angle); int32_t cos_m = cos_lookup(min_angle);
    GPoint m_p_start = { c.x + (m_d_start * sin_m) / TRIG_MAX_RATIO, c.y - (m_d_start * cos_m) / TRIG_MAX_RATIO };
    GPoint m_p_end   = { c.x + (m_d_end * sin_m) / TRIG_MAX_RATIO, c.y - (m_d_end * cos_m) / TRIG_MAX_RATIO };
    graphics_draw_line(ctx, m_p_start, m_p_end);

    // 時マーカー（短針）
    GColor h_color = PBL_IF_COLOR_ELSE(GColorRed, txt_color);
    graphics_context_set_stroke_color(ctx, h_color);
    graphics_context_set_stroke_width(ctx, 6); 
    int32_t sin_h = sin_lookup(hour_angle); int32_t cos_h = cos_lookup(hour_angle);
    int h_start_r = safe_r + 2; int h_end_r = h_orbit_r - 2; 
    GPoint h_p_start = { c.x + (h_start_r * sin_h) / TRIG_MAX_RATIO, c.y - (h_start_r * cos_h) / TRIG_MAX_RATIO };
    GPoint h_p_end   = { c.x + (h_end_r * sin_h) / TRIG_MAX_RATIO, c.y - (h_end_r * cos_h) / TRIG_MAX_RATIO };
    graphics_draw_line(ctx, h_p_start, h_p_end); 
  }

  graphics_context_set_stroke_width(ctx, 1);

  // --- シャッター羽の描画 ---
  for (int i = 0; i < 12; i++) {
    GColor fill = get_wing_fill_color(i);
    graphics_context_set_fill_color(ctx, fill);

    GColor stroke = gcolor_legible_over(fill); 
    if (PBL_IF_COLOR_ELSE(true, false)) {
      if (gcolor_equal(stroke, GColorBlack)) stroke = GColorDarkGray;
      else stroke = GColorLightGray;
    }
    graphics_context_set_stroke_color(ctx, stroke);

    s_dynamic_path->points[0] = get_inner_vertex(i, r, angle, c);
    s_dynamic_path->points[1] = get_ray_far_point(i, r, angle, c);
    s_dynamic_path->points[2] = get_ray_far_point(i + 1, r, angle, c);
    s_dynamic_path->points[3] = get_inner_vertex(i + 1, r, angle, c);
    
    gpath_draw_filled(ctx, s_dynamic_path); 
    gpath_draw_outline(ctx, s_dynamic_path);
  }

  // --- 手前のテキスト描画エリア ---
  if (r > 40) {
    graphics_context_set_fill_color(ctx, txt_color);
    graphics_context_set_stroke_color(ctx, txt_color);
    graphics_context_set_text_color(ctx, txt_color);
    
    if (current_mode == 0) {
      // デジタル時計モード
      int f_h = s_current_f_size;
      int char_spacing = f_h / 2; 
      int gap = (f_h / 15 < 2) ? 2 : (f_h / 15);
      int c1 = c.x - (gap / 2) - (char_spacing / 2); int c0 = c1 - char_spacing;
      int c2 = c.x + (gap / 2) + (char_spacing / 2); int c3 = c2 + char_spacing;
      int text_y = c.y - (f_h / 2) - (f_h / 6);
      
      graphics_draw_text(ctx, s_time_chars[0], s_time_font, GRect(c0 - (f_h / 2), text_y, f_h, f_h+20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, s_time_chars[1], s_time_font, GRect(c1 - (f_h / 2), text_y, f_h, f_h+20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, s_time_chars[2], s_time_font, GRect(c2 - (f_h / 2), text_y, f_h, f_h+20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, s_time_chars[3], s_time_font, GRect(c3 - (f_h / 2), text_y, f_h, f_h+20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

      int line_top_y = c.y - (r / 2); int line_bottom_y = c.y + (r / 2);

      graphics_draw_text(ctx, s_date_str, s_info_font, GRect(0, line_top_y - 12, b.size.w, 15), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      int info_y = line_bottom_y - 2;
      int boot_x = c.x - 44; 
      graphics_fill_rect(ctx, GRect(boot_x + 1, info_y + 9, 9, 2), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 1, info_y + 3, 3, 6), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 4, info_y + 5, 3, 4), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 7, info_y + 7, 3, 2), 0, GCornerNone);

      graphics_draw_text(ctx, s_steps_str, s_info_font, GRect(c.x - 30, info_y - 2, 45, 15), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      draw_weather_icon(ctx, (GPoint){c.x + 16, info_y + 4}, txt_color, s_settings.weather);
      graphics_draw_text(ctx, "/", s_info_font, GRect(c.x + 24, info_y - 2, 10, 15), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      draw_weather_icon(ctx, (GPoint){c.x + 36, info_y + 4}, txt_color, s_settings.tomorrow_weather);

    } else {
      // アナログ時計モード
      int top_steps_y = c.y - 27; 
      int mid_date_y  = c.y - 8;  
      int bot_weather_y = c.y + 11; 

      int boot_x = c.x - 22; 
      graphics_fill_rect(ctx, GRect(boot_x + 1, top_steps_y + 11, 9, 2), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 1, top_steps_y + 5,  3, 6), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 4, top_steps_y + 7,  3, 4), 0, GCornerNone); 
      graphics_fill_rect(ctx, GRect(boot_x + 7, top_steps_y + 9,  3, 2), 0, GCornerNone); 
      graphics_draw_text(ctx, s_steps_str, s_info_font, GRect(boot_x + 14, top_steps_y, 40, 15), GTextOverflowModeFill, GTextAlignmentLeft, NULL);

      graphics_draw_text(ctx, s_date_str, s_info_font, GRect(0, mid_date_y, b.size.w, 15), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

      draw_weather_icon(ctx, (GPoint){c.x - 10, bot_weather_y + 6}, txt_color, s_settings.weather);
      graphics_draw_text(ctx, "/", s_info_font, GRect(c.x - 4, bot_weather_y, 10, 15), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      draw_weather_icon(ctx, (GPoint){c.x + 10, bot_weather_y + 6}, txt_color, s_settings.tomorrow_weather);
    }
  }
}

static void main_window_load(Window *window) {
  Layer *wl = window_get_root_layer(window); GRect fb = layer_get_bounds(wl);
  s_info_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DENKICHIP_SUBSET_12));
  s_canvas_layer = layer_create(fb); 
  layer_set_update_proc(s_canvas_layer, update_proc); 
  layer_add_child(wl, s_canvas_layer);
  s_dynamic_path = gpath_create(&s_path_info);
  unobstructed_area_service_unsubscribe(); 
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) { .did_change = unobstructed_did_change }, NULL);
  s_last_battery = battery_state_service_peek();
  
  apply_theme_colors(); 
  adjust_layout(fb.size.h); 
  update_info();
}

//0.8秒待ってから実行される「遅延シャッター関数」
static void delayed_shutter_cb(void *data) {
  if (s_pending_biz_update) {
    trigger_shutter_internal(false);
  }
}

//修正版フォーカス監視
static void focus_handler(bool in_focus) {
  s_in_focus = in_focus;
  
  if (in_focus) {
    evaluate_context_modes();
    
    // モード切り替えがある場合、ポップアップが完全に消えるのを「1.0秒」待ってから変形する
    if (s_pending_biz_update) {
      app_timer_register(1000, delayed_shutter_cb, NULL); 
    }
  }
}

static void main_window_unload(Window *window) {
  if (s_time_font) fonts_unload_custom_font(s_time_font); 
  fonts_unload_custom_font(s_info_font);
  gpath_destroy(s_dynamic_path); layer_destroy(s_canvas_layer);
  unobstructed_area_service_unsubscribe(); 
}

int main(void) {
  load_settings(); 
  app_message_register_inbox_received(inbox_received_handler); 
  app_message_open(256, 256);
  
  s_main_window = window_create(); 
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load, 
    .unload = main_window_unload
  }); 
  window_stack_push(s_main_window, true);
  
  accel_tap_service_subscribe(handle_tap); 
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick); 
  battery_state_service_subscribe(handle_battery);
  
  //画面フォーカスの監視を開始する
  app_focus_service_subscribe(focus_handler);
  
  app_event_loop();
  
  //終了時に監視を解除する
  app_focus_service_unsubscribe();
  
  battery_state_service_unsubscribe(); 
  tick_timer_service_unsubscribe(); 
  accel_tap_service_unsubscribe(); 
  window_destroy(s_main_window);
}