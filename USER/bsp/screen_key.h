/**
 * @file screen_key.h
 * @brief Single key debounce for waveform-period switching.
 */
#ifndef SCREEN_KEY_H
#define SCREEN_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void ScreenKey_Init(void);
void ScreenKey_Task(uint32_t now_ms);
uint8_t ScreenKey_TakeSinglePress(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_KEY_H */
