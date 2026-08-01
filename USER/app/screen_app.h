/**
 * @file screen_app.h
 * @brief Application coordinator for the screen MCU.
 */
#ifndef SCREEN_APP_H
#define SCREEN_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_protocol.h"

void ScreenApp_Init(void);
void ScreenApp_Task(void);
void ScreenApp_SetFrame(const ScreenDataFrame *frame);
void ScreenApp_SetDisplayMode(ScreenDisplayMode mode);
void ScreenApp_SetWavePeriods(ScreenWavePeriodMode periods);
ScreenDisplayMode ScreenApp_GetDisplayMode(void);
ScreenWavePeriodMode ScreenApp_GetWavePeriods(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_APP_H */
