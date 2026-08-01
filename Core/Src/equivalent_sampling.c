/**
 * @file equivalent_sampling.c
 * @brief Equivalent-sampling single-period reconstruction (adapted to fixed
 *        1 MHz internal ADC, no TIM clock divider).
 *
 * 参考工程 AD9226_driver 的等效采样思路:
 *  - 相位折叠: 按基频相位把多个真实周期的采样点折叠到 256 个相位格;
 *  - 同格平均: 减小随机噪声;
 *  - 空格插值: 首尾相连线性插值补齐;
 *  - 基频相位对齐: DFT bin-1 估计基频相位并旋转, 让显示波形静止稳定。
 */
#include "equivalent_sampling.h"

#include <limits.h>
#include <math.h>
#include <string.h>

/* 复用工作区, 避免占用较大函数栈 */
static int64_t g_bin_sums[ES_PERIOD_POINTS];
static uint16_t g_bin_counts[ES_PERIOD_POINTS];
static int16_t g_align_buffer[ES_PERIOD_POINTS];

/* 32 位无符号数表示一个周期, 溢出后自然回到下一周期起点 */
static uint32_t EsPhaseIncrement(uint32_t fundamental_hz, uint32_t fs_hz)
{
    const uint64_t numerator = ((uint64_t)fundamental_hz << 32);
    return (uint32_t)(numerator / fs_hz);
}

static int16_t EsRoundedAverage(int64_t sum, uint16_t count)
{
    if (sum >= 0)
    {
        return (int16_t)((sum + count / 2U) / count);
    }
    return (int16_t)((sum - count / 2U) / count);
}

/* 只估计基频相位, 不删除重建波形中的谐波分量 */
static void EsAlignFundamental(int16_t period[ES_PERIOD_POINTS])
{
    float sine_component = 0.0f;
    float cosine_component = 0.0f;
    uint16_t i;

    for (i = 0U; i < ES_PERIOD_POINTS; ++i)
    {
        const float angle = 2.0f * ES_PI * (float)i / (float)ES_PERIOD_POINTS;
        sine_component += (float)period[i] * sinf(angle);
        cosine_component += (float)period[i] * cosf(angle);
    }

    {
        const float phase = atan2f(cosine_component, sine_component);
        const int32_t shift =
            (int32_t)lroundf(-phase * (float)ES_PERIOD_POINTS / (2.0f * ES_PI));

        for (i = 0U; i < ES_PERIOD_POINTS; ++i)
        {
            int32_t source = ((int32_t)i + shift) % (int32_t)ES_PERIOD_POINTS;
            if (source < 0)
            {
                source += ES_PERIOD_POINTS;
            }
            g_align_buffer[i] = period[source];
        }
    }

    memcpy(period, g_align_buffer, sizeof(g_align_buffer));
}

bool EsReconstructPeriod(
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t fundamental_hz,
    uint32_t fs_hz,
    int16_t period[ES_PERIOD_POINTS])
{
    uint32_t increment;
    uint32_t phase = 0UL;
    uint16_t bin;
    uint16_t coverage = 0U;
    uint32_t i;

    if ((samples == NULL) || (period == NULL) ||
        (fundamental_hz == 0UL) || (fs_hz == 0UL) || (sample_count < 4U))
    {
        return false;
    }

    memset(g_bin_sums, 0, sizeof(g_bin_sums));
    memset(g_bin_counts, 0, sizeof(g_bin_counts));

    increment = EsPhaseIncrement(fundamental_hz, fs_hz);

    /* 将跨越多个真实周期的采样点折叠到同一个基频周期 */
    for (i = 0U; i < sample_count; ++i)
    {
        bin = (uint16_t)(phase >> 24);
        g_bin_sums[bin] += samples[i];
        ++g_bin_counts[bin];
        phase += increment;
    }

    for (bin = 0U; bin < ES_PERIOD_POINTS; ++bin)
    {
        if (g_bin_counts[bin] != 0U)
        {
            period[bin] = EsRoundedAverage(g_bin_sums[bin], g_bin_counts[bin]);
            ++coverage;
        }
    }

    if (coverage < 2U)
    {
        return false;
    }

    /* 空相位格使用首尾相连的线性插值补齐 */
    for (bin = 0U; bin < ES_PERIOD_POINTS; ++bin)
    {
        uint16_t left_distance = 1U;
        uint16_t right_distance = 1U;
        uint16_t left;
        uint16_t right;
        int32_t weighted;

        if (g_bin_counts[bin] != 0U)
        {
            continue;
        }

        while (g_bin_counts[
                   (uint16_t)((bin + ES_PERIOD_POINTS - left_distance) & 0x00FFU)] == 0U)
        {
            ++left_distance;
        }
        while (g_bin_counts[
                   (uint16_t)((bin + right_distance) & 0x00FFU)] == 0U)
        {
            ++right_distance;
        }

        left = (uint16_t)((bin + ES_PERIOD_POINTS - left_distance) & 0x00FFU);
        right = (uint16_t)((bin + right_distance) & 0x00FFU);
        weighted = (int32_t)period[left] * right_distance +
                   (int32_t)period[right] * left_distance;

        period[bin] = (int16_t)(weighted / (left_distance + right_distance));
    }

    EsAlignFundamental(period);
    return true;
}

bool NormalReconstructPeriod(
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t freq_hz,
    uint32_t fs_hz,
    int16_t period[ES_PERIOD_POINTS])
{
    float samples_per_period;
    uint32_t start = 0U;
    uint32_t search_limit;
    uint16_t i;

    if ((samples == NULL) || (period == NULL) ||
        (freq_hz == 0U) || (fs_hz == 0U) || (sample_count < 4U))
    {
        return false;
    }

    samples_per_period = (float)fs_hz / (float)freq_hz;
    if ((samples_per_period < 2.0f) || (samples_per_period > (float)sample_count))
    {
        return false;
    }

    /* 找最近的上升过零点, 锁定相位, 使波形静止 */
    search_limit = (uint32_t)(samples_per_period * 2.0f + 0.5f);
    if (search_limit > (sample_count - 1U))
    {
        search_limit = sample_count - 1U;
    }
    for (i = 0U; (i + 1U) < sample_count && i < search_limit; ++i)
    {
        if ((samples[i] <= 0) && (samples[i + 1U] > 0))
        {
            start = i;
            break;
        }
    }

    /* 用精确的小数周期长度重建, 避免首尾不连续造成频谱泄漏 */
    for (i = 0U; i < ES_PERIOD_POINTS; ++i)
    {
        const float phase = (float)i / (float)ES_PERIOD_POINTS;
        const float fidx = (float)start + phase * samples_per_period;
        const uint32_t idx0 = (uint32_t)fidx;
        const float frac = fidx - (float)idx0;
        const uint32_t s0 = idx0 % sample_count;
        const uint32_t s1 = (idx0 + 1U) % sample_count;
        const float v0 = (float)samples[s0];
        const float v1 = (float)samples[s1];
        int32_t v = (int32_t)(v0 + frac * (v1 - v0) + 0.5f);

        if (v < INT16_MIN) { v = INT16_MIN; }
        else if (v > INT16_MAX) { v = INT16_MAX; }
        period[i] = (int16_t)v;
    }

    return true;
}

/* 小信号显示放大: 振幅 <64 码时放大到 64 码, 只影响视觉, 不改变测量 */
#define ES_DISPLAY_MIN_RANGE_CODES 64u

static void EsNormalizeSmallPeriod(const int16_t *in, int16_t *out)
{
    int16_t min_v = in[0];
    int16_t max_v = in[0];
    int32_t range;
    uint16_t i;

    for (i = 1U; i < ES_PERIOD_POINTS; i++)
    {
        if (in[i] < min_v) { min_v = in[i]; }
        if (in[i] > max_v) { max_v = in[i]; }
    }

    range = (int32_t)max_v - (int32_t)min_v;
    if (range < 1) { range = 1; }

    if (range >= (int32_t)ES_DISPLAY_MIN_RANGE_CODES)
    {
        memcpy(out, in, (size_t)ES_PERIOD_POINTS * sizeof(int16_t));
        return;
    }

    {
        const int32_t target = (int32_t)ES_DISPLAY_MIN_RANGE_CODES;
        const int32_t offset = ((int32_t)max_v + (int32_t)min_v) / 2;
        for (i = 0U; i < ES_PERIOD_POINTS; i++)
        {
            int32_t y = offset + (((int32_t)in[i] - offset) * target) / range;
            if (y > 2047) { y = 2047; }
            else if (y < -2048) { y = -2048; }
            out[i] = (int16_t)y;
        }
    }
}

/* 256 点 → 768 点上采样 (加权插值) */
static void EsUpsample768(const uint16_t *src, uint16_t *dst)
{
    const uint16_t in_points = ES_PERIOD_POINTS;
    const uint16_t out_points = ES_DISPLAY_POINTS;
    uint16_t i;

    for (i = 0U; i < out_points; i++)
    {
        uint32_t pos = ((uint32_t)i * (in_points - 1u)) / (out_points - 1u);
        uint32_t rem = ((uint32_t)i * (in_points - 1u)) % (out_points - 1u);
        uint16_t idx = (uint16_t)pos;
        uint32_t w1 = (out_points - 1u) - rem;
        uint32_t w2 = rem;
        uint32_t v;

        if ((idx + 1u) < in_points)
        {
            v = ((uint32_t)src[idx] * w1 + (uint32_t)src[idx + 1u] * w2) / (out_points - 1u);
        }
        else
        {
            v = src[idx];
        }
        dst[i] = (uint16_t)v;
    }
}

uint16_t EsFillDisplayBuffer(
    const int16_t *period,
    uint8_t cycles,
    uint16_t *out)
{
    static int16_t norm_period[ES_PERIOD_POINTS];
    static uint16_t offset_period[ES_PERIOD_POINTS];
    uint16_t i;

    if ((period == NULL) || (out == NULL) ||
        ((cycles != 1U) && (cycles != 3U)))
    {
        return 0U;
    }

    EsNormalizeSmallPeriod(period, norm_period);

    if (cycles == 1U)
    {
        for (i = 0U; i < ES_PERIOD_POINTS; i++)
        {
            offset_period[i] = (uint16_t)(norm_period[i] + 2048);
        }
        EsUpsample768(offset_period, out);
    }
    else
    {
        for (i = 0U; i < ES_DISPLAY_POINTS; i++)
        {
            out[i] = (uint16_t)(norm_period[i % ES_PERIOD_POINTS] + 2048);
        }
    }

    return ES_DISPLAY_POINTS;
}
