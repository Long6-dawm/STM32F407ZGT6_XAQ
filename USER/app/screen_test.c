#include "screen_test.h"

#include "screen_config.h"
#include "wave_table.h"
#include <stddef.h>
#include <string.h>

static uint32_t s_last_update_ms;
static uint32_t s_sequence;

static uint32_t ScreenTest_Checksum32(const uint8_t *data, uint16_t len)
{
  uint32_t sum = 0u;
  uint16_t i;

  for (i = 0u; i < len; i++)
  {
    sum += data[i];
  }

  return sum;
}

static void ScreenTest_FillWave(ScreenDataFrame *frame, uint32_t tick)
{
  (void)tick;

  frame->wave_count = WAVE_TABLE_LENGTH;
  memcpy(frame->wave, g_wave_table, sizeof(g_wave_table));
}

static void ScreenTest_FillFft(ScreenDataFrame *frame, uint32_t tick)
{
  uint16_t i;
  uint16_t shift = (uint16_t)((tick / 500u) % 16u);

  frame->fft_count = SCREEN_FFT_MAX_POINTS;
  for (i = 0u; i < frame->fft_count; i++)
  {
    uint16_t bin = i;
    uint16_t value = 8u;

    if ((bin > (18u + shift)) && (bin < (26u + shift)))
    {
      value = (uint16_t)(3200u - (uint16_t)((bin - (22u + shift)) * (bin - (22u + shift)) * 80u));
    }
    else if ((bin > 66u) && (bin < 74u))
    {
      value = (uint16_t)(900u - (uint16_t)((bin - 70u) * (bin - 70u) * 40u));
    }
    else if ((bin > 110u) && (bin < 118u))
    {
      value = (uint16_t)(420u - (uint16_t)((bin - 114u) * (bin - 114u) * 20u));
    }

    frame->fft[i] = value;
  }
}

void ScreenTest_Init(void)
{
  s_last_update_ms = 0u;
  s_sequence = 0u;
}

uint8_t ScreenTest_Generate(uint32_t now_ms,
                            ScreenDisplayMode mode,
                            ScreenWavePeriodMode periods,
                            ScreenDataFrame *out)
{
  uint16_t payload_len;

  if (out == NULL)
  {
    return 0u;
  }

  if ((s_sequence != 0u) && ((now_ms - s_last_update_ms) < SCREEN_TEST_UPDATE_INTERVAL_MS))
  {
    return 0u;
  }

  s_last_update_ms = now_ms;
  s_sequence++;

  memset(out, 0, sizeof(*out));
  out->magic = SCREEN_FRAME_MAGIC;
  out->version = SCREEN_FRAME_VERSION;
  out->byte_len = sizeof(*out);
  out->sequence = s_sequence;
  out->frequency_mhz = 10500000u;
  out->vpp_uv = 826600u;
  out->vrms_uv = 212100u;
  out->harmonic_freq_mhz[0] = out->frequency_mhz;
  out->harmonic_freq_mhz[1] = out->frequency_mhz * 3u;
  out->harmonic_freq_mhz[2] = out->frequency_mhz * 5u;
  out->harmonic_rms_uv[0] = out->vrms_uv;
  out->harmonic_rms_uv[1] = out->vrms_uv / 7u;
  out->harmonic_rms_uv[2] = out->vrms_uv / 13u;
  out->mode = (uint8_t)mode;
  out->wave_periods = (uint8_t)periods;

  ScreenTest_FillWave(out, now_ms);
  ScreenTest_FillFft(out, now_ms);

  payload_len = (uint16_t)offsetof(ScreenDataFrame, checksum);
  out->checksum = ScreenTest_Checksum32((const uint8_t *)out, payload_len);
  return 1u;
}
