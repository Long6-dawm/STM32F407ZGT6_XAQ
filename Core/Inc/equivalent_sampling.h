#ifndef EQUIVALENT_SAMPLING_H
#define EQUIVALENT_SAMPLING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ES_EQ_SAMPLE_COUNT   4096u   /* 一帧采集点数 */
#define ES_PERIOD_POINTS     256u    /* 重建单周期点数 */
#define ES_DISPLAY_POINTS    768u    /* 屏幕波形显示点数 */
#define ES_PI                3.14159265358979323846f

/*
 * 等效采样重构: 把 sample_count 个真实采样点按基频相位折叠到
 * ES_PERIOD_POINTS 个相位格, 同格平均去噪, 空格线性插值补齐,
 * 最后做基频相位对齐(DFT bin-1), 使显示波形相位稳定不跳动。
 * 成功返回 true, period 为 256 点单周期 signed 波形(相对 2048 零点)。
 */
bool EsReconstructPeriod(
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t fundamental_hz,
    uint32_t fs_hz,
    int16_t period[ES_PERIOD_POINTS]);

/* 普通采样兜底: 上升过零锁相 + 小数索引线性插值重采样一个周期到 256 点。 */
bool NormalReconstructPeriod(
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t freq_hz,
    uint32_t fs_hz,
    int16_t period[ES_PERIOD_POINTS]);

/* 把 256 点 signed 单周期填成屏幕用 uint16 波形(加 2048 回 ADC 码)。
 * cycles=1: 上采样到 768 点; cycles=3: 重复 3 次。
 * 返回实际填充点数(768), 失败返回 0。 */
uint16_t EsFillDisplayBuffer(
    const int16_t *period,
    uint8_t cycles,
    uint16_t *out);

#ifdef __cplusplus
}
#endif

#endif /* EQUIVALENT_SAMPLING_H */
