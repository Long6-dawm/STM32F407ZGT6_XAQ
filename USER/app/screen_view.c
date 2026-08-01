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


/* FFT 离散线谱: 横轴 0~500kHz 线性, 谱线高度 ∝ 分量峰值幅度, 自动量程 */
static void ScreenView_RenderFft(const ScreenDataFrame *frame)
{
  uint16_t line_pts = SCREEN_FFT_VISIBLE_POINTS;
  uint32_t max_amp = 1u;
  uint16_t i;
  static uint8_t s_pixel_buf[SCREEN_FFT_VISIBLE_POINTS];

  /* 基线置底 */
  for (i = 0u; i < line_pts; i++)
  {
    s_pixel_buf[i] = 0u;
  }

  /* 自动量程: 取各有效分量的峰值幅度最大值 */
  for (i = 0u; i < 3u; i++)
  {
    if (frame->harmonic_freq_mhz[i] != 0u)
    {
      uint32_t peak = ((uint32_t)frame->harmonic_rms_uv[i] * 14142u) / 10000u; /* rms*√2 */
      if (peak > max_amp)
      {
        max_amp = peak;
      }
    }
  }

  /* 每条有效谱线: 在频率线性位置画尖峰 */
  for (i = 0u; i < 3u; i++)
  {
    uint32_t freq_hz;
    uint32_t peak;
    uint16_t x;
    uint16_t h;
    int32_t k;

    if (frame->harmonic_freq_mhz[i] == 0u)
    {
      continue;
    }

    freq_hz = frame->harmonic_freq_mhz[i] / 1000u;
    peak = ((uint32_t)frame->harmonic_rms_uv[i] * 14142u) / 10000u;
    h = (uint16_t)((uint64_t)peak * (uint32_t)(SCREEN_CURVE_HEIGHT - 2u)) / max_amp;
    if (h > SCREEN_CURVE_HEIGHT)
    {
      h = SCREEN_CURVE_HEIGHT;
    }

    x = (uint16_t)((uint64_t)freq_hz * (uint32_t)(line_pts - 1u)) / SCREEN_FFT_FREQ_MAX_HZ;
    if (x >= line_pts)
    {
      x = (uint16_t)(line_pts - 1u);
    }

    /* 尖峰: x-半宽 ~ x+半宽 */
    for (k = -(int32_t)SCREEN_FFT_LINE_HALF_WIDTH; k <= (int32_t)SCREEN_FFT_LINE_HALF_WIDTH; k++)
    {
      int32_t idx = (int32_t)x + k;
      if (idx < 0)
      {
        idx = 0;
      }
      if (idx >= (int32_t)line_pts)
      {
        idx = (int32_t)line_pts - 1;
      }
      if ((uint16_t)h > s_pixel_buf[(uint16_t)idx])
      {
        s_pixel_buf[(uint16_t)idx] = (uint8_t)h;
      }
    }
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
  if (frame == NULL)
  {
    return;
  }

  ScreenView_SetWavePeriods(periods);
  ScreenView_RenderMeasurements(frame);
 
  if (mode == SCREEN_MODE_FFT)
  {
    ScreenView_RenderFft(frame);
  }
  else
  {
    ScreenView_RenderWave(frame, periods);
  }
}
