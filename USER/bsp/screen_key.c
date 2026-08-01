#include "screen_key.h"

#include "main.h"

#define SCREEN_KEY_DEBOUNCE_MS 30u

static uint8_t s_stable_level;
static uint8_t s_last_raw_level;
static uint8_t s_single_press;
static uint32_t s_last_change_ms;

static uint8_t ScreenKey_ReadSingleRaw(void)
{
  return (HAL_GPIO_ReadPin(Single_MCU_GPIO_Port, Single_MCU_Pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

void ScreenKey_Init(void)
{
  s_stable_level = ScreenKey_ReadSingleRaw();
  s_last_raw_level = s_stable_level;
  s_single_press = 0u;
  s_last_change_ms = 0u;
}

void ScreenKey_Task(uint32_t now_ms)
{
  uint8_t raw = ScreenKey_ReadSingleRaw();

  if (raw != s_last_raw_level)
  {
    s_last_raw_level = raw;
    s_last_change_ms = now_ms;
    return;
  }

  if ((raw != s_stable_level) && ((now_ms - s_last_change_ms) >= SCREEN_KEY_DEBOUNCE_MS))
  {
    s_stable_level = raw;
    if (s_stable_level != 0u)
    {
      s_single_press = 1u;
    }
  }
}

uint8_t ScreenKey_TakeSinglePress(void)
{
  uint8_t pressed = s_single_press;
  s_single_press = 0u;
  return pressed;
}
