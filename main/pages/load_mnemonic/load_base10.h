#ifndef LOAD_BASE10_H
#define LOAD_BASE10_H

#include <lvgl.h>

void load_base10_page_create(lv_obj_t *parent, void (*return_cb)(void));
void load_base10_page_show(void);
void load_base10_page_hide(void);
void load_base10_page_destroy(void);

#endif // LOAD_BASE10_H
