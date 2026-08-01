/**
 * @file wave_render.c
 * @brief Phase-locked slicing of real ADC samples into 768 display points.
 *
 * Stability measures:
 *  - Stable DC reference: (min + max) / 2, independent of capture phase.
 *  - Rising center-crossing trigger to phase-lock consecutive frames.
 *  - Slice REAL consecutive samples (1x or 3x periods) instead of repeating
 *    a resampled single period, so the waveform is inherently continuous.
 */
#include "wave_render.h"

#include <string.h>

/* 输出平滑参数 (可调) */
#define WAVE_SMOOTH_TAPS   5u
#define WAVE_SMOOTH_PASSES 2u

/* 静态临时缓冲, 避免占主栈 */
static uint16_t s_sm_buf[WAVE_DISPLAY_POINTS];

static void WaveRender_Smooth(uint16_t *data, uint16_t len)
{
  uint8_t pass;
  uint16_t i;

  for (pass = 0u; pass < WAVE_SMOOTH_PASSES; pass++)
  {
    memcpy(s_sm_buf, data, (size_t)len * sizeof(uint16_t));
    for (i = 0u; i < len; i++)
    {
      uint32_t lo = (i >= 2u) ? s_sm_buf[i - 2u] : s_sm_buf[0u];
      uint32_t a  = (i >= 1u) ? s_sm_buf[i - 1u] : s_sm_buf[0u];
      uint32_t c  = s_sm_buf[i];
      uint32_t b  = (i + 1u < len) ? s_sm_buf[i + 1u] : s_sm_buf[len - 1u];
      uint32_t hi = (i + 2u < len) ? s_sm_buf[i + 2u] : s_sm_buf[len - 1u];

      data[i] = (uint16_t)((lo + a + c + b + hi) / (uint32_t)WAVE_SMOOTH_TAPS);
    }
  }
}

uint16_t WaveRender_Build(const uint16_t *raw, uint32_t raw_len,
                          float freq_hz, uint8_t periods,
                          uint16_t *out)
{
  float period_pts;
  uint32_t start = 0u;
  uint32_t n_src;
  uint32_t i;
  uint32_t k;
  uint32_t mn;
  uint32_t mx;

  if ((raw == NULL) || (out == NULL) || (raw_len == 0u))
  {
    return 0u;
  }

  /* 兜底: 无法确定基频时直接输出原始缓冲前 WAVE_DISPLAY_POINTS 点 */
  if (freq_hz <= 0.0f)
  {
    uint16_t count = (uint16_t)((raw_len < WAVE_DISPLAY_POINTS) ? raw_len : WAVE_DISPLAY_POINTS);
    memcpy(out, raw, (size_t)count * sizeof(uint16_t));
    WaveRender_Smooth(out, count);
    return count;
  }

  period_pts = 1000000.0f / freq_hz;
  if (period_pts < 2.0f)
  {
    period_pts = 2.0f;
  }

  /* 稳定 DC 基准: (min+max)/2, 与采样相位无关, 跨帧稳定 */
  mn = raw[0];
  mx = raw[0];
  for (k = 1u; k < raw_len; k++)
  {
    if (raw[k] < mn) { mn = raw[k]; }
    if (raw[k] > mx) { mx = raw[k]; }
  }
  {
    uint32_t center = (mn + mx) / 2u;

    /* 上升过零触发, 锁定相位 */
    for (k = 1u; k < raw_len; k++)
    {
      if ((raw[k - 1u] <= center) && (raw[k] > center))
      {
        start = k;
        break;
      }
    }
  }

  /* 需要截取的源采样点数: 1 个周期 或 3 个周期 */
  n_src = (uint32_t)((periods == 1u) ? (period_pts + 0.5f) : (3.0f * period_pts + 0.5f));
  if (n_src < 2u)
  {
    n_src = 2u;
  }

  /* 截取 n_src 个连续真实采样点, 线性重采样到 WAVE_DISPLAY_POINTS 点 */
  for (i = 0u; i < WAVE_DISPLAY_POINTS; i++)
  {
    float pos = ((float)i / (float)(WAVE_DISPLAY_POINTS - 1u)) * (float)(n_src - 1u);
    uint32_t i0 = (uint32_t)pos;
    float frac = pos - (float)i0;
    uint32_t i1 = i0 + 1u;
    float v0 = (float)raw[(start + i0) % raw_len];
    float v1 = (float)raw[(start + i1) % raw_len];
    float val = v0 * (1.0f - frac) + v1 * frac;

    out[i] = (uint16_t)(val + 0.5f);
  }

  WaveRender_Smooth(out, WAVE_DISPLAY_POINTS);

  return WAVE_DISPLAY_POINTS;
}
