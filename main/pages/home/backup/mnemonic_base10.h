/*
 * Mnemonic Base10 Backup Page
 * Displays Base10 digits for backup
 */

#ifndef MNEMONIC_BASE10_H
#define MNEMONIC_BASE10_H

#include <lvgl.h>

void mnemonic_base10_page_create(lv_obj_t *parent, void (*return_cb)(void));
void mnemonic_base10_page_show(void);
void mnemonic_base10_page_hide(void);
void mnemonic_base10_page_destroy(void);

#endif // MNEMONIC_BASE10_H
