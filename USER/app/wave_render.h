/**
 * @file wave_render.h
 * @brief Runtime single-period extraction and 768-point waveform generation.
 */
#ifndef WAVE_RENDER_H
#define WAVE_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WAVE_PERIOD_LUT_POINTS 256u
#define WAVE_DISPLAY_POINTS    768u

/**
 * @brief 从 ADC 原始缓冲提取一个完整周期并重采样, 再按模式映射为 768 个显示点.
 * @param raw      原始 ADC 数据 (uint16 计数值)
 * @param raw_len  raw 长度
 * @param freq_hz  基波频率 (Hz), 用于计算每周期采样点数
 * @param periods  显示模式: 1 = 单周期拉伸铺满, 3 = 三周期循环
 * @param out      输出缓冲, 至少 WAVE_DISPLAY_POINTS 长度
 * @return 输出点数 (正常为 WAVE_DISPLAY_POINTS; freq_hz 无效时返回原始缓冲截取点数)
 */
uint16_t WaveRender_Build(const uint16_t *raw, uint32_t raw_len,
                          float freq_hz, uint8_t periods,
                          uint16_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WAVE_RENDER_H */
