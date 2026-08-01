#include "screen_app.h"

#include "hmi_tjc.h"
#include "screen_config.h"
#include "screen_key.h"
#include "screen_test.h"
#include "screen_view.h"
#include <string.h>

static ScreenDataFrame s_frame;
static ScreenDisplayMode s_mode = SCREEN_MODE_WAVE;
static ScreenWavePeriodMode s_wave_periods = (ScreenWavePeriodMode)SCREEN_DEFAULT_WAVE_PERIODS;
static uint32_t s_last_refresh_ms;
static uint32_t s_last_status_ms;
static uint32_t s_rendered_sequence;

static void ScreenApp_OnTouch(uint8_t page_id, uint8_t component_id, uint8_t event_type);

void ScreenApp_SetDisplayMode(ScreenDisplayMode mode)
{
  s_mode = mode;
  ScreenView_SetMode(s_mode);
}

void ScreenApp_SetWavePeriods(ScreenWavePeriodMode periods)
{
  if ((periods != SCREEN_WAVE_PERIOD_1) && (periods != SCREEN_WAVE_PERIOD_3))
  {
    periods = (ScreenWavePeriodMode)SCREEN_DEFAULT_WAVE_PERIODS;
  }

  s_wave_periods = periods;
  ScreenView_SetWavePeriods(s_wave_periods);
}

void ScreenApp_SetFrame(const ScreenDataFrame *frame)
{
  if (frame == NULL)
  {
    return;
  }

  memcpy(&s_frame, frame, sizeof(s_frame));
  if (frame->mode <= SCREEN_MODE_FFT)
  {
    ScreenApp_SetDisplayMode((ScreenDisplayMode)frame->mode);
  }
}

ScreenDisplayMode ScreenApp_GetDisplayMode(void)
{
  return s_mode;
}

ScreenWavePeriodMode ScreenApp_GetWavePeriods(void)
{
  return s_wave_periods;
}

void ScreenApp_Init(void)
{
  memset(&s_frame, 0, sizeof(s_frame));
  s_last_refresh_ms = 0u;
  s_last_status_ms = 0u;
  s_rendered_sequence = 0u;
  s_wave_periods = (ScreenWavePeriodMode)SCREEN_DEFAULT_WAVE_PERIODS;

  HMI_Init();
  HMI_SetTouchCallback(ScreenApp_OnTouch);
  ScreenKey_Init();
  ScreenTest_Init();
  ScreenView_Init();
  ScreenApp_SetWavePeriods(s_wave_periods);
}

void ScreenApp_Task(void)
{
  uint32_t now = HAL_GetTick();

  HMI_Task();
  ScreenKey_Task(now);

  if (ScreenKey_TakeSinglePress() != 0u)
  {
    ScreenApp_SetWavePeriods((s_wave_periods == SCREEN_WAVE_PERIOD_1) ? SCREEN_WAVE_PERIOD_3 : SCREEN_WAVE_PERIOD_1);
  }

#if SCREEN_TEST_ENABLE
  if (s_frame.magic != SCREEN_FRAME_MAGIC)
  {
    static ScreenDataFrame test_frame;
    if (ScreenTest_Generate(now, s_mode, s_wave_periods, &test_frame))
    {
      ScreenApp_SetFrame(&test_frame);
    }
  }
#endif

  if ((now - s_last_status_ms) >= SCREEN_STATUS_INTERVAL_MS)
  {
    s_last_status_ms = now;
    ScreenView_SetLinkState((s_frame.magic == SCREEN_FRAME_MAGIC) ? 1u : 0u, s_frame.sequence);
  }

  if ((s_frame.magic == SCREEN_FRAME_MAGIC) &&
      ((s_frame.sequence != s_rendered_sequence) ||
       ((now - s_last_refresh_ms) >= SCREEN_REFRESH_INTERVAL_MS)))
  {
    s_last_refresh_ms = now;
    s_rendered_sequence = s_frame.sequence;
    ScreenView_RenderFrame(&s_frame, s_mode, s_wave_periods);
  }
}

static void ScreenApp_OnTouch(uint8_t page_id, uint8_t component_id, uint8_t event_type)
{
  (void)page_id;

  if (event_type == 0u)
  {
    return;
  }

  if (component_id == 1u)
  {
    ScreenApp_SetDisplayMode(SCREEN_MODE_WAVE);
  }
  else if (component_id == 2u)
  {
    ScreenApp_SetDisplayMode(SCREEN_MODE_FFT);
  }
}
