
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_PERF_MONITOR     0
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_DRAW_SW          1
#define LV_USE_FS_STDIO         0
#define LV_USE_DEV_CUSTOM       0

#define LV_COLOR_DEPTH          16
#define LV_COLOR_SCREEN_TRANSP  0

#define LV_TICK_CUSTOM          0

#define LV_USE_USER_DATA        1
#define LV_USE_LAYER            1
#define LV_MEM_SIZE             (48U * 1024U)  // 48KB heap

#define LV_USE_TIMER            1
#define LV_USE_LABEL            1
#define LV_USE_BTN              1
#define LV_USE_DISPLAY          1

// Add other widgets as needed
#define LV_USE_BTNMATRIX        1
#define LV_USE_SWITCH           1
#define LV_USE_SLIDER           1
#define LV_USE_ARC              1
#define LV_USE_IMG              1

#endif // LV_CONF_H
