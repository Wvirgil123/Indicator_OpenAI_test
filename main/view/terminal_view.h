#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include "view_data.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
#define ENABLE_SENSOR_LOG 0
#define ENABLE_TIME_LOG   0
#define SENSOR_HISTORY_DATA_DEBUG 0
int terminal_view_init(void);

#ifdef __cplusplus
}
#endif

#endif
