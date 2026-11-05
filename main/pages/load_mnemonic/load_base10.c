// Decimal Base10 Mnemonic Loader page (Universal Adaptive Input)

#include "load_base10.h"
#include "../../ui/numeric_keypad.h"
#include "../../ui/dialog.h"
#include "../../ui/theme_widgets.h"
#include "../../ui/word_selector.h"
#include "../../utils/secure_mem.h"
#include "../shared/kef_decrypt_page.h"
#include "../shared/key_confirmation.h"
#include "../home/home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGULAR_BLOCK_LEN 4
#define MAX_TOTAL_DIGITS 148

static lv_obj_t *load_base10_screen = NULL;
static ui_numeric_keypad_t *base10_keypad = NULL;
static void (*return_callback)(void) = NULL;

// Адаптивные переменные страницы
static char entered_digits[MAX_TOTAL_DIGITS + 1];
static int current_block_index = 0;
static int total_digits = 148;
static int total_blocks = 37;
static int envelope_bytes_len = 61;

static void success_from_key_confirmation_cb(void) {
  key_confirmation_page_destroy();
  load_base10_page_destroy();
  home_page_create(lv_screen_active());
  home_page_show();
}

static void success_from_kef_decrypt_cb(const uint8_t *data, size_t len) {
  key_confirmation_page_create(
      lv_screen_active(), return_callback,
      success_from_key_confirmation_cb, (const char *)data, len);
  key_confirmation_page_show();
  kef_decrypt_page_destroy();
}

static void return_from_kef_decrypt_cb(void) {
  kef_decrypt_page_destroy();
  void (*callback)(void) = return_callback;
  load_base10_page_destroy();
  if (callback) {
    callback();
  }
}

static void process_and_decrypt(void) {
  // 1. ВАЛИДАЦИЯ CRC ISO 7064 MOD 97-10 (Строго слева направо по динамической длине)
  uint32_t check_remainder = 0;
  for (int i = 0; i < total_digits; i++) {
    check_remainder = (check_remainder * 10 + (entered_digits[i] - '0')) % 97;
  }

  // Если остаток не равен 1 — вылетаем в Load Mnemonic с ошибкой
  if (check_remainder != 1) {
    dialog_show_error_timeout("CRC Check Failure!", NULL, 0);
    
    void (*callback)(void) = return_callback;
    load_base10_page_destroy();
    if (callback) {
      callback();
    }
    return;
  }

  // 2. РАСШИФРОВКА С АДАПТИВНЫМ КАНАЛОМ (45 или 61 байт)
  size_t envelope_len = envelope_bytes_len; 
  uint8_t *envelope = calloc(1, envelope_len);
  if (!envelope) return;

  // Отрезаем последние 2 цифры CRC (идем от total_digits - 3 до 0)
  for (int i = total_digits - 3; i >= 0; i--) {
    uint32_t carry = entered_digits[i] - '0';
    for (int j = (int)envelope_len - 1; j >= 0; j--) {
      uint32_t current = envelope[j] * 10 + carry;
      envelope[j] = current % 256;
      carry = current / 256;
    }
  }

  load_base10_page_hide();

  kef_decrypt_page_create(lv_screen_active(), return_from_kef_decrypt_cb,
                          success_from_kef_decrypt_cb, envelope, envelope_len);
  kef_decrypt_page_show();
  
  free(envelope);

  dialog_show_success_timeout("CRC Valid!", NULL, 1500);
  //dialog_show_message("Success", "CRC Valid!"); 
}

static void open_block_keypad(void);

static void block_submit_cb(uint32_t value, void *user_data) {
  (void)user_data;
  
  int expected_len = REGULAR_BLOCK_LEN;

  char temp_buf[8]; // Буфер с запасом под 4 цифры + \0
  snprintf(temp_buf, sizeof(temp_buf), "%0*u", expected_len, value);
  
  // Копируем строго 4 символа цифр без затирания памяти нуль-терминатором!
  int write_offset = current_block_index * REGULAR_BLOCK_LEN;
  memcpy(entered_digits + write_offset, temp_buf, expected_len);

  // Надежно закрываем строку Си в текущей финальной точке
  int current_string_length = write_offset + expected_len;
  entered_digits[current_string_length] = '\0';

  current_block_index++;

  // Проверяем достижение финала по адаптивной переменной total_blocks
  if (current_block_index >= total_blocks) {
    process_and_decrypt();
  } else {
    open_block_keypad();
  }
}

static void block_cancel_cb(void *user_data) {
  (void)user_data;
  if (return_callback) {
    return_callback();
  }
}

static void open_block_keypad(void) {
  static char title[64];
  int expected_len = REGULAR_BLOCK_LEN;

  snprintf(title, sizeof(title), "Block %d/%d (%d digits)", 
           current_block_index + 1, total_blocks, expected_len);

  ui_numeric_keypad_config_t config = {
      .title = title,
      .initial_value = 0,
      .min_value = 1, // Открывает клавиатуру кристально чистой
      .max_value = 9999,
      .max_digits = expected_len,
      .invalid_message = "Enter ALL digits!", 
      .submit_cb = block_submit_cb,
      .cancel_cb = block_cancel_cb,
  };
  ui_numeric_keypad_open(&base10_keypad, &config);
}

static void word_count_back_cb(void) {
  if (return_callback) {
    return_callback();
  }
}

// Этот коллбэк сработает, когда пользователь выберет 12 или 24 слова на старте страницы
static void word_count_selected_cb(int word_count) {
  current_block_index = 0;
  memset(entered_digits, 0, sizeof(entered_digits));

  // Настраиваем математику прошивки под выбранный тип кошелька
  if (word_count == 12) {
    envelope_bytes_len = 45; // Конверт 12 слов
    total_digits = 112;      // 110 цифр данных + 2 CRC
    total_blocks = 28;       // 28 идеальных блоков по 4 цифры
  } else {
    envelope_bytes_len = 61; // Конверт 24 слов
    total_digits = 148;      // 146 цифр данных + 2 CRC
    total_blocks = 37;       // 37 идеальных блоков по 4 цифры
  }

  // Прячем контейнер селектора слов и запускаем нашу клавиатуру блоков цифр!
  if (load_base10_screen) {
    lv_obj_add_flag(load_base10_screen, LV_OBJ_FLAG_HIDDEN);
  }
  open_block_keypad();
}

void load_base10_page_create(lv_obj_t *parent, void (*return_cb)(void)) {
  if (!parent) return;

  return_callback = return_cb;
  current_block_index = 0;
  memset(entered_digits, 0, sizeof(entered_digits));

  load_base10_screen = theme_create_page_container(parent);
  
  // Вызываем штатный селектор автора: он сам нарисует выбор "12 Words" / "24 Words"
  ui_word_count_selector_create(load_base10_screen, word_count_back_cb,
                                word_count_selected_cb);
}

void load_base10_page_show(void) {
  if (load_base10_screen)
    lv_obj_clear_flag(load_base10_screen, LV_OBJ_FLAG_HIDDEN);
}

void load_base10_page_hide(void) {
  if (load_base10_screen)
    lv_obj_add_flag(load_base10_screen, LV_OBJ_FLAG_HIDDEN);
}

void load_base10_page_destroy(void) {
  ui_numeric_keypad_close(&base10_keypad);
  if (load_base10_screen) {
    lv_obj_del(load_base10_screen);
    load_base10_screen = NULL;
  }
  return_callback = NULL;
  current_block_index = 0;
  memset(entered_digits, 0, sizeof(entered_digits));
}
