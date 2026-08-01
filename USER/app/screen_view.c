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

static uint8_t ScreenView_ScaleU16(uint16_t sample, uint16_t max_sample, int32_t offset, int32_t gain_q8, uint8_t invert)
{
  int32_t y;

  if (max_sample == 0u)
  {
    max_sample = 1u;
  }

  y = (int32_t)(((uint32_t)sample * SCREEN_CURVE_HEIGHT) / max_sample);
  if (invert != 0u)
  {
    y = (int32_t)SCREEN_CURVE_HEIGHT - y;
  }

  y = ((y * gain_q8) >> 8) + offset;
  if (y < 0)
  {
    y = 0;
  }
  if (y > (int32_t)SCREEN_CURVE_HEIGHT)
  {
    y = (int32_t)SCREEN_CURVE_HEIGHT;
  }

  return (uint8_t)y;
}

static uint16_t ScreenView_MaxU16(const uint16_t *data, uint16_t count)
{
  uint16_t max_value = 1u;
  uint16_t i;

  for (i = 0u; i < count; i++)
  {
    if (data[i] > max_value)
    {
      max_value = data[i];
    }
  }

  return max_value;
}

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
  uint16_t max_value;
  uint16_t i;
  static uint8_t s_pixel_buf[SCREEN_WAVE_VISIBLE_POINTS];
  static uint16_t s_max_sm;

  (void)periods;

  points = (visible_count < SCREEN_WAVE_VISIBLE_POINTS) ? visible_count : SCREEN_WAVE_VISIBLE_POINTS;
  if (points == 0u)
  {
    return;
  }

  max_value = ScreenView_MaxU16(frame->wave, visible_count);
  /* 垂直幅度 EMA 平滑, 防止每帧最大幅值抖动导致上下跳动 */
  if (s_max_sm == 0u)
  {
    s_max_sm = max_value;
  }
  else
  {
    s_max_sm = (uint16_t)(((uint32_t)s_max_sm * 7u + (uint32_t)max_value * 3u) / 10u);
  }
  if (s_max_sm == 0u)
  {
    s_max_sm = 1u;
  }

  for (i = 0u; i < points; i++)
  {
    s_pixel_buf[i] = ScreenView_ScaleU16(frame->wave[i], s_max_sm, SCREEN_WAVE_Y_OFFSET, SCREEN_WAVE_Y_GAIN_Q8, 0u);
  }

  HMI_Addt_Send(SCREEN_WAVE_CTRL, 0u, s_pixel_buf, points);
}


static void ScreenView_RenderFft(const ScreenDataFrame *frame)
{
  uint16_t points = (frame->fft_count < SCREEN_FFT_VISIBLE_POINTS) ? frame->fft_count : SCREEN_FFT_VISIBLE_POINTS;
  uint16_t max_value = ScreenView_MaxU16(frame->fft, frame->fft_count);
  uint16_t i;
  static uint8_t s_pixel_buf[SCREEN_FFT_VISIBLE_POINTS];

  if (points == 0u)
  {
    return;
  }

  for (i = 0u; i < points; i++)
  {
    uint16_t src = (uint16_t)(((uint32_t)i * frame->fft_count) / points);
    s_pixel_buf[i] = ScreenView_ScaleU16(frame->fft[src], max_value, SCREEN_FFT_Y_OFFSET, SCREEN_FFT_Y_GAIN_Q8, 0u);
  }
  HMI_Addt_Send(SCREEN_FFT_CTRL, 0u, s_pixel_buf, points);
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
