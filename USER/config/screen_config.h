/**
 * @file screen_config.h
 * @brief Screen layout and refresh tuning knobs.
 */
#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include <stdint.h>

#define SCREEN_PAGE_NAME "page0"

#define SCREEN_WAVE_CTRL "s_wave"
#define SCREEN_FFT_CTRL "s_fft"

#define SCREEN_CURVE_HEIGHT 210u
#define SCREEN_WAVE_VISIBLE_POINTS 768u
#define SCREEN_FFT_VISIBLE_POINTS 768u
#define SCREEN_FFT_FREQ_MAX_HZ 500000u   /* FFT 横轴上限(固定量程/自动量程的封顶) */
#define SCREEN_FFT_AUTO_MAX_FACTOR_Q10 13u   /* 自动量程: 最高谐波频率 × 1.3 */
#define SCREEN_FFT_AUTO_MIN_HZ 100000u       /* 自动量程: 全宽下限(防止低频时轴太窄) */

#define SCREEN_DEFAULT_WAVE_PERIODS 3u

#define SCREEN_WAVE_Y_OFFSET 0
#define SCREEN_WAVE_Y_GAIN_Q8 256
#define SCREEN_FFT_Y_OFFSET 0
#define SCREEN_FFT_Y_GAIN_Q8 256
#define SCREEN_FFT_LINE_HALF_WIDTH 1u   /* 每条谱线尖峰半宽(点数) */

#define SCREEN_REFRESH_INTERVAL_MS 300u
#define SCREEN_STATUS_INTERVAL_MS 500u
#define SCREEN_HMI_BOOT_DELAY_MS 500u

#define SCREEN_TEST_ENABLE 1u
#define SCREEN_TEST_UPDATE_INTERVAL_MS 200u

#define SCREEN_COLOR_WHITE 65535u
#define SCREEN_COLOR_GREEN 2016u
#define SCREEN_COLOR_YELLOW 65504u

#endif /* SCREEN_CONFIG_H */
