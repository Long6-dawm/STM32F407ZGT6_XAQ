#include "screen_view.h"

#include "hmi_tjc.h"
#include "screen_config.h"
#include <stdio.h>
#include "screen_app.h"

static void ScreenView_FormatUv(uint32_t uv, char *value, size_t value_size, char *unit, size_t unit_size)
{
  if (uv >= 1000000u)
  {
    (void)snprintf(value, value_size, "%lu.%03lu", (unsigned long)(uv / 1000000u), (unsigned long)((uv % 1000000u) / 1000u));
    (void)snprintf(unit, unit_size, "V");
  }
  else if (uv >= 1000u)
  {
    (void)snprintf(value, value_size, "%lu.%03lu", (unsigned long)(uv / 1000u), (unsigned long)(uv % 1000u));
    (void)snprintf(unit, unit_size, "mV");
  }
  else
  {
    (void)snprintf(value, value_size, "%lu", (unsigned long)uv);
    (void)snprintf(unit, unit_size, "uV");
  }
}

static void ScreenView_FormatMhz(uint32_t mhz, char *value, size_t value_size, char *unit, size_t unit_size)
{
  if (mhz >= 1000000000u)
  {
    (void)snprintf(value, value_size, "%lu.%03lu", (unsigned long)(mhz / 1000000000u), (unsigned long)((mhz % 1000000000u) / 1000000u));
    (void)snprintf(unit, unit_size, "MHz");
  }
  else if (mhz >= 1000000u)
  {
    (void)snprintf(value, value_size, "%lu.%03lu", (unsigned long)(mhz / 1000000u), (unsigned long)((mhz % 1000000u) / 1000u));
    (void)snprintf(unit, unit_size, "kHz");
  }
  else
  {
    (void)snprintf(value, value_size, "%lu.%03lu", (unsigned long)(mhz / 1000u), (unsigned long)(mhz % 1000u));
    (void)snprintf(unit, unit_size, "Hz");
  }
}

static uint16_t ScreenView_RangeU16(const uint16_t *data, uint16_t count, uint16_t *out_min)
{
  uint16_t min_value;
  uint16_t max_value;
  uint16_t i;

  if (count == 0u)
  {
    if (out_min != NULL)
    {
      *out_min = 0u;
    }
    return 1u;
  }

  min_value = data[0];
  max_value = data[0];
  for (i = 1u; i < count; i++)
  {
    if (data[i] < min_value)
    {
      min_value = data[i];
    }
    if (data[i] > max_value)
    {
      max_value = data[i];
    }
  }

  if (out_min != NULL)
  {
    *out_min = min_value;
  }
  if (max_value == min_value)
  {
    max_value = min_value + 1u;
  }
  return (uint16_t)(max_value - min_value);
}

/* 波形 Y 轴自动量程的平滑状态 (文件级, 供 ResetWaveScale 重置) */
static uint32_t s_wave_range_sm;
static uint32_t s_wave_min_sm;

static void ScreenView_RenderMeasurements(const ScreenDataFrame *frame)
{
  char value[24];
  char unit[8];
  uint8_t i;

  ScreenView_FormatUv(frame->vpp_uv, value, sizeof(value), unit, sizeof(unit));
  HMI_SetText("Vpp_unit", unit);
  HMI_SetText("t_vpp", value);

  ScreenView_FormatUv(frame->vrms_uv, value, sizeof(value), unit, sizeof(unit));
  HMI_SetText("vrms_unit", unit);
  HMI_SetText("t_rms", value);

  ScreenView_FormatMhz(frame->frequency_mhz, value, sizeof(value), unit, sizeof(unit));
  HMI_SetText("f1_unit", unit);
  HMI_SetText("t_f1", value);

  for (i = 0u; i < 3u; i++)
  {
    char obj[12];
    char text_obj[12];
    char unit_obj[16];

    (void)snprintf(obj, sizeof(obj), "f0%u", (unsigned int)(i + 1u));
    (void)snprintf(text_obj, sizeof(text_obj), "v0%u", (unsigned int)(i + 1u));
    (void)snprintf(unit_obj, sizeof(unit_obj), "f0%u_unit", (unsigned int)(i + 1u));
    ScreenView_FormatMhz(frame->harmonic_freq_mhz[i], value, sizeof(value), unit, sizeof(unit));
    HMI_SetText(obj, value);
    HMI_SetText(unit_obj, unit);

    (void)snprintf(unit_obj, sizeof(unit_obj), "v0%u_unit", (unsigned int)(i + 1u));
    ScreenView_FormatUv(frame->harmonic_rms_uv[i], value, sizeof(value), unit, sizeof(unit));
    HMI_SetText(text_obj, value);
    HMI_SetText(unit_obj, unit);
  }
}

static void ScreenView_RenderWave(const ScreenDataFrame *frame, ScreenWavePeriodMode periods)
{
  uint16_t visible_count = frame->wave_count;
  uint16_t points;
  uint16_t min_value;
  uint16_t range;
  uint16_t i;
  static uint8_t s_pixel_buf[SCREEN_WAVE_VISIBLE_POINTS];
  static uint8_t s_last_periods;

  points = (visible_count < SCREEN_WAVE_VISIBLE_POINTS) ? visible_count : SCREEN_WAVE_VISIBLE_POINTS;
  if (points == 0u)
  {
    return;
  }

  /* 动态范围(min-max)归一化 + 居中: 峰峰铺满曲线, 上下留边距 */
  range = ScreenView_RangeU16(frame->wave, visible_count, &min_value);

  /* 范围与最小值的 EMA 平滑, 防止每帧抖动导致波形上下平移/缩放"呼吸" */
  if (s_wave_range_sm == 0u)
  {
    s_wave_range_sm = range;
    s_wave_min_sm = min_value;
  }
  else
  {
    s_wave_range_sm = (s_wave_range_sm * 7u + (uint32_t)range * 3u) / 10u;
    s_wave_min_sm = (s_wave_min_sm * 7u + (uint32_t)min_value * 3u) / 10u;
  }
  if (s_wave_range_sm == 0u)
  {
    s_wave_range_sm = 1u;
  }

  for (i = 0u; i < points; i++)
  {
    int32_t d = (int32_t)frame->wave[i] - (int32_t)s_wave_min_sm;
    int32_t y;
    int32_t y_adj;

    if (d < 0)
    {
      d = 0;
    }
    y = ((int32_t)((uint32_t)d * (SCREEN_CURVE_HEIGHT - 20u)) / (int32_t)s_wave_range_sm) + 10;
    y_adj = (((int32_t)y * SCREEN_WAVE_Y_GAIN_Q8) >> 8) + SCREEN_WAVE_Y_OFFSET;

    if (y_adj < 0)
    {
      y_adj = 0;
    }
    if (y_adj > (int32_t)SCREEN_CURVE_HEIGHT)
    {
      y_adj = (int32_t)SCREEN_CURVE_HEIGHT;
    }
    s_pixel_buf[i] = (uint8_t)y_adj;
  }

  /* 切换显示周期数时清屏, 避免残留上一模式的波形 */
  if (periods != (ScreenWavePeriodMode)s_last_periods)
  {
    s_last_periods = (uint8_t)periods;
    HMI_ClearWave(SCREEN_WAVE_CTRL, 255u);
  }

  HMI_Addt_Send(SCREEN_WAVE_CTRL, 0u, s_pixel_buf, points);
}

void ScreenView_ResetWaveScale(void)
{
  s_wave_range_sm = 0u;
  s_wave_min_sm = 0u;
}


/* FFT 连续频谱: 512 点(0~500kHz) 线性映射到满宽, 高度 ∝ 幅度, 自动量程.
 * 采用整条连续曲线(像 s_wave), 是 s_fft vscope 控件能可靠渲染的形式。 */
static void ScreenView_RenderFft(const ScreenDataFrame *frame)
{
  uint16_t line_pts = SCREEN_FFT_VISIBLE_POINTS;
  uint16_t src_count = frame->fft_count;
  uint32_t max_mag = 1u;
  uint16_t i;
  static uint8_t s_pixel_buf[SCREEN_FFT_VISIBLE_POINTS];

  if (src_count == 0u)
  {
    return;
  }
  if (src_count > SCREEN_FFT_MAX_POINTS)
  {
    src_count = SCREEN_FFT_MAX_POINTS;
  }

  /* 自动量程 */
  for (i = 0u; i < src_count; i++)
  {
    if (frame->fft[i] > max_mag)
    {
      max_mag = frame->fft[i];
    }
  }
  if (max_mag == 0u)
  {
    max_mag = 1u;
  }

  /* src_count 点 → 满宽线性插值, 高度 ∝ 幅度 */
  for (i = 0u; i < line_pts; i++)
  {
    float pos = (float)i * (float)(src_count - 1u) / (float)(line_pts - 1u);
    uint32_t i0 = (uint32_t)pos;
    uint32_t i1 = (i0 + 1u < src_count) ? (i0 + 1u) : i0;
    float frac = pos - (float)i0;
    uint32_t v = (uint32_t)((float)frame->fft[i0] * (1.0f - frac) +
                            (float)frame->fft[i1] * frac + 0.5f);
    uint32_t y = ((uint64_t)v * (uint32_t)(SCREEN_CURVE_HEIGHT - 2u)) / max_mag;

    if (y > SCREEN_CURVE_HEIGHT)
    {
      y = SCREEN_CURVE_HEIGHT;
    }
    s_pixel_buf[i] = (uint8_t)y;
  }

  /* 每次清空重画, 防止曲线缓冲堆积/重复 */
  HMI_ClearWave(SCREEN_FFT_CTRL, 255u);
  HMI_Addt_Send(SCREEN_FFT_CTRL, 0u, s_pixel_buf, line_pts);
}

void ScreenView_Init(void)
{
  HAL_Delay(SCREEN_HMI_BOOT_DELAY_MS);
  HMI_GotoPage(SCREEN_PAGE_NAME);
  HMI_ClearWave(SCREEN_WAVE_CTRL, 255u);
  HMI_ClearWave(SCREEN_FFT_CTRL, 255u);
  ScreenView_SetMode(SCREEN_MODE_WAVE);
  ScreenView_SetWavePeriods((ScreenWavePeriodMode)SCREEN_DEFAULT_WAVE_PERIODS);
}

void ScreenView_SetMode(ScreenDisplayMode mode)
{
  (void)mode;
}

void ScreenView_SetWavePeriods(ScreenWavePeriodMode periods)
{
  HMI_SetText("T_num", (periods == SCREEN_WAVE_PERIOD_1) ? "1" : "3");
}

void ScreenView_SetLinkState(uint8_t linked, uint32_t sequence)
{
  (void)linked;
  (void)sequence;
}

void ScreenView_RenderFrame(const ScreenDataFrame *frame, ScreenDisplayMode mode, ScreenWavePeriodMode periods)
{
  (void)mode;

  if (frame == NULL)
  {
    return;
  }

  ScreenView_SetWavePeriods(periods);
  ScreenView_RenderMeasurements(frame);

  /* 时域波形始终刷新；FFT 离散谱线每帧同时刷新 (参考 AD9226 工程行为) */
  ScreenView_RenderWave(frame, periods);
  ScreenView_RenderFft(frame);
}
