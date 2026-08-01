/**
 * @file screen_protocol.h
 * @brief SPI payload shared with the STM32H723 FFT board.
 */
#ifndef SCREEN_PROTOCOL_H
#define SCREEN_PROTOCOL_H

#include <stdint.h>

#define SCREEN_FRAME_MAGIC 0x32434445u
#define SCREEN_FRAME_VERSION 1u
#define SCREEN_WAVE_MAX_POINTS 768u
#define SCREEN_FFT_MAX_POINTS 256u

typedef enum
{
  SCREEN_MODE_WAVE = 0u,
  SCREEN_MODE_FFT = 1u,
} ScreenDisplayMode;

typedef enum
{
  SCREEN_WAVE_PERIOD_1 = 1u,
  SCREEN_WAVE_PERIOD_3 = 3u,
} ScreenWavePeriodMode;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t byte_len;
  uint32_t sequence;
  uint32_t frequency_mhz;
  uint32_t vpp_uv;
  uint32_t vrms_uv;
  uint32_t harmonic_freq_mhz[3];
  uint32_t harmonic_rms_uv[3];
  uint16_t wave_count;
  uint16_t fft_count;
  uint8_t mode;
  uint8_t wave_periods;
  uint8_t reserved[2];
  uint16_t wave[SCREEN_WAVE_MAX_POINTS];
  uint16_t fft[SCREEN_FFT_MAX_POINTS];
  uint32_t checksum;
} ScreenDataFrame;

#endif /* SCREEN_PROTOCOL_H */
