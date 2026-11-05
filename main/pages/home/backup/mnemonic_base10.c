// Mnemonic Base10 Backup Page

#include "mnemonic_base10.h"
#include "../../../core/key.h"
#include "../../../qr/encoder.h"
#include "../../shared/kef_encrypt_page.h"
#include "../../../ui/dialog.h"
#include "../../../ui/theme.h"
#include "../../../ui/theme_widgets.h"
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static lv_obj_t *base10_screen = NULL;
static void (*return_callback)(void) = NULL;

static void back_cb(lv_event_t *e) {
  (void)e;
  if (return_callback)
    return_callback();
}

// Коллбэк успеха шифрования — строго по сигнатуре из mnemonic_qr.c
static void base10_encrypt_success_cb(const char *id, const uint8_t *envelope, size_t len) {
  (void)id;

  if (!envelope || len == 0 || !base10_screen) {
    kef_encrypt_page_destroy();
    if (return_callback) return_callback();
    return;
  }

  // === 1. ДИНАМИЧЕСКИЙ РАСЧЕТ РАЗМЕРОВ ПОД ТИП КОШЕЛЬКА ===
  int data_digits_len = 146;   // По умолчанию для 24 слов (61 байт конверта)
  int total_digits_len = 148;  // 146 данных + 2 CRC

  if (len == 45) {             // Если в памяти кошелек на 12 слов (45 байт конверта)
    data_digits_len = 110;     // 110 данных
    total_digits_len = 112;    // 110 данных + 2 CRC
  }

  // Выделяем память под рабочие буферы с учетом динамических размеров
  uint8_t *buffer = malloc(len);
  char *converted_str = malloc(total_digits_len + 1);
  if (!buffer || !converted_str) {
    free(buffer); free(converted_str);
    kef_encrypt_page_destroy();
    if (return_callback) return_callback();
    return;
  }
  memcpy(buffer, envelope, len);

  // Перевод BigInt в строку (крутим цикл строго до вычисленной data_digits_len)
  for (int i = 0; i < data_digits_len; i++) {
    uint32_t remainder = 0;
    for (size_t j = 0; j < len; j++) {
      uint32_t current = remainder * 256 + buffer[j];
      buffer[j] = current / 10;
      remainder = current % 10;
    }
    converted_str[i] = '0' + remainder;
  }
  free(buffer);

  // Закрываем шторку ввода ПИН-кода
  kef_encrypt_page_destroy();

  // === 2. УНИВЕРСАЛЬНОЕ ДОБАВЛЕНИЕ CRC ПО ISO 7064 MOD 97-10 ===
  uint32_t crc_remainder = 0;
  // Проходим строго по реальной длине данных (110 или 146 цифр)
  for (int i = 0; i < data_digits_len; i++) {
    crc_remainder = (crc_remainder * 10 + (converted_str[i] - '0')) % 97;
  }
  // Дописываем два виртуальных нуля для остатка по стандарту
  crc_remainder = (crc_remainder * 10 + 0) % 97;
  crc_remainder = (crc_remainder * 10 + 0) % 97;

  // Рассчитываем итоговые две цифры чексуммы
  uint32_t check_digits = 98 - crc_remainder;
  if (check_digits == 98) check_digits = 1; // Коррекция по стандарту

  // Дописываем две цифры CRC в самый конец нашей адаптивной строки
  converted_str[data_digits_len] = '0' + (check_digits / 10);
  converted_str[data_digits_len + 1] = '0' + (check_digits % 10);
  converted_str[total_digits_len] = '\0';

  // === 3. КРАСИВОЕ АДАПТИВНОЕ ФОРМАТИРОВАНИЕ НА ЭКРАН ===
  // Выделяем буфер под отформатированный текст (блоки строго по 4 цифры)
  char *base10_buf = malloc(total_digits_len * 2);
  if (!base10_buf) {
    free(converted_str);
    if (return_callback) return_callback();
    return;
  }

  int buf_idx = 0;
  for (int i = 0; i < total_digits_len; i++) {
    if (i > 0) {
      if (i % 16 == 0) {
        base10_buf[buf_idx++] = '\n'; // Перенос строки каждые 4 блока для красоты
      } else if (i % 4 == 0) {
        base10_buf[buf_idx++] = ' ';  // Пробел между блоками
      }
    }
    base10_buf[buf_idx++] = converted_str[i];
  }
  base10_buf[buf_idx] = '\0';
  free(converted_str);

  // === 4. ВЫВОД НА ЭКРАН СИМУЛЯТОРА ===
  lv_obj_t *label = theme_create_label(base10_screen, base10_buf, false);
  lv_obj_set_style_text_font(label, theme_font_medium(), 0);
  lv_obj_set_style_text_color(label, primary_color(), 0);
  
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, lv_pct(85));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_line_space(label, 14, 0);

  free(base10_buf);

  lv_obj_t *hint = theme_create_label(base10_screen, "Tap to return", true);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -theme_default_padding());
}

// Коллбэк отмены ПИН-кода
static void base10_encrypt_return_cb(void) {
  kef_encrypt_page_destroy();
  if (return_callback)
    return_callback();
}

void mnemonic_base10_page_create(lv_obj_t *parent, void (*return_cb)(void)) {
  if (!parent) return;
  return_callback = return_cb;

  base10_screen = theme_create_page_container(parent);
  lv_obj_add_flag(base10_screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(base10_screen, back_cb, LV_EVENT_CLICKED, NULL);
  theme_create_page_title(base10_screen, "Base10 Backup");

  // Извлекаем сид один в один как в mnemonic_qr.c
  size_t compact_seed_len = 0;
  char *mnemonic_data = NULL;
  
  // Получаем указатель или копию мнемоники (безопасный вызов ядра)
  if (!key_get_mnemonic(&mnemonic_data) || !mnemonic_data) {
    dialog_show_error_timeout("No mnemonic loaded", NULL, 0);
    if (return_callback) return_callback();
    return;
  }
  
  uint8_t *compact_seed_data = mnemonic_to_compact_seedqr(mnemonic_data, &compact_seed_len);
  
  // Очищаем временную строку мнемоники
  memset(mnemonic_data, 0, strlen(mnemonic_data));
  free(mnemonic_data);

  if (!compact_seed_data || compact_seed_len == 0) {
    dialog_show_error_timeout("No data to encrypt", NULL, 0);
    if (return_callback) return_callback();
    return;
  }

  // ВЫЗОВ ШИФРОВАНИЯ ИЗ KEF_ENCRYPT_PAGE (Как в QR, без варнингов)
  kef_encrypt_page_create(lv_screen_active(), base10_encrypt_return_cb,
                          base10_encrypt_success_cb, compact_seed_data,
                          compact_seed_len, NULL);

  free(compact_seed_data); 
}

void mnemonic_base10_page_show(void) {
  if (base10_screen)
    lv_obj_clear_flag(base10_screen, LV_OBJ_FLAG_HIDDEN);
}

void mnemonic_base10_page_hide(void) {
  if (base10_screen)
    lv_obj_add_flag(base10_screen, LV_OBJ_FLAG_HIDDEN);
}

void mnemonic_base10_page_destroy(void) {
  if (base10_screen) {
    lv_obj_del(base10_screen);
    base10_screen = NULL;
  }
  return_callback = NULL;
}
