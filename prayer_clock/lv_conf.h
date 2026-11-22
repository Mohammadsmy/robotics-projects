/* LVGL 9 settings for the prayer clock. Anything not set here keeps LVGL's default. */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB   /* plain malloc, so big blocks can land in PSRAM */
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE                  /* only the UI task ever calls LVGL */
#define LV_DEF_REFR_PERIOD 33

#define LV_USE_BIDI 1
#define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_RTL
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_FONT_COMPRESSED 1              /* the generated Arabic fonts are compressed */

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1               /* Wi-Fi / bot icons */
#define LV_USE_LOG 0

#endif
#endif
