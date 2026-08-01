/**
 * @file screen_view.h
 * @brief TJC page rendering for periodic-signal measurements.
 */
#ifndef SCREEN_VIEW_H
#define SCREEN_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_protocol.h"

void ScreenView_Init(void);
void ScreenView_SetMode(ScreenDisplayMode mode);
void ScreenView_SetWavePeriods(ScreenWavePeriodMode periods);
void ScreenView_SetLinkState(uint8_t linked, uint32_t sequence);
void ScreenView_RenderFrame(const ScreenDataFrame *frame, ScreenDisplayMode mode, ScreenWavePeriodMode periods);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_VIEW_H */
