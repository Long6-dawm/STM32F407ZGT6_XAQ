/**
 * @file screen_test.h
 * @brief Local demo data source used before the H723 link is ready.
 */
#ifndef SCREEN_TEST_H
#define SCREEN_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_protocol.h"
#include <stdint.h>

void ScreenTest_Init(void);
uint8_t ScreenTest_Generate(uint32_t now_ms,
                            ScreenDisplayMode mode,
                            ScreenWavePeriodMode periods,
                            ScreenDataFrame *out);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_TEST_H */
