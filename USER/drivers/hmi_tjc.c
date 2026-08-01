#include "hmi_tjc.h"

#include "usart.h"
#include <stdio.h>
#include <string.h>

#define HMI_RX_BUF_SIZE 256u
#define HMI_TX_TIMEOUT_MS 20u

static const uint8_t kHmiEndMarker[3] = {0xFFu, 0xFFu, 0xFFu};

static HMI_OnTouchCb s_touch_cb;
static uint8_t s_rx_byte;
static uint8_t s_rx_buf[HMI_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

int __io_putchar(int ch)
{
  uint8_t byte = (uint8_t)ch;
  HAL_UART_Transmit(&huart1, &byte, 1u, HMI_TX_TIMEOUT_MS);
  return ch;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next;

  if (huart->Instance != USART1)
  {
    return;
  }

  next = (uint16_t)((s_rx_head + 1u) % HMI_RX_BUF_SIZE);
  if (next != s_rx_tail)
  {
    s_rx_buf[s_rx_head] = s_rx_byte;
    s_rx_head = next;
  }

  HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1u);
}

static void HMI_SendBytes(const uint8_t *data, uint16_t len)
{
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HMI_TX_TIMEOUT_MS);
}

void HMI_SendCmd(const char *cmd)
{
  if (cmd == NULL)
  {
    return;
  }

  HMI_SendBytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  HMI_SendBytes(kHmiEndMarker, 3u);
}

void HMI_SetText(const char *obj, const char *text)
{
  char buf[128];

  if ((obj == NULL) || (text == NULL))
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", obj, text);
  HMI_SendCmd(buf);
}

void HMI_SetValue(const char *obj, int32_t value)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "%s.val=%ld", obj, (long)value);
  HMI_SendCmd(buf);
}

void HMI_SetColor565(const char *obj, uint16_t color)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "%s.pco=%u", obj, (unsigned int)color);
  HMI_SendCmd(buf);
}

void HMI_AddWavePoint(const char *obj, uint8_t channel, uint8_t value)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "add %s.id,%u,%u", obj, (unsigned int)channel, (unsigned int)value);
  HMI_SendCmd(buf);
}

void HMI_Addt_Send(const char *curve_id, uint8_t ch, const uint8_t *data, uint16_t len)
{
  char cmd_buf[32];

  if ((curve_id == NULL) || (data == NULL) || (len == 0u))
  {
    return;
  }

  (void)snprintf(cmd_buf, sizeof(cmd_buf), "addt %s.id,%u,%u", curve_id, (unsigned int)ch, (unsigned int)len);
  HMI_SendCmd(cmd_buf);

  HAL_Delay(50);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HMI_TX_TIMEOUT_MS);
  HAL_Delay(20);
}

void HMI_ClearWave(const char *obj, uint8_t channel_mask)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "cle %s.id,%u", obj, (unsigned int)channel_mask);
  HMI_SendCmd(buf);
}

void HMI_GotoPage(const char *page_name)
{
  char buf[64];

  if (page_name == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "page %s", page_name);
  HMI_SendCmd(buf);
}

void HMI_SetTouchCallback(HMI_OnTouchCb cb)
{
  s_touch_cb = cb;
}

static bool HMI_RxReadByte(uint8_t *out)
{
  if (s_rx_head == s_rx_tail)
  {
    return false;
  }

  *out = s_rx_buf[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1u) % HMI_RX_BUF_SIZE);
  return true;
}

static bool HMI_RxReadFrame(uint8_t *frame, uint16_t frame_size, uint16_t *actual_len)
{
  uint16_t pos = 0u;
  uint8_t b;
  uint8_t ff_count = 0u;

  while (HMI_RxReadByte(&b))
  {
    if (pos >= frame_size)
    {
      return false;
    }

    frame[pos++] = b;
    ff_count = (b == 0xFFu) ? (uint8_t)(ff_count + 1u) : 0u;
    if (ff_count >= 3u)
    {
      *actual_len = pos;
      return true;
    }
  }

  return false;
}

static void HMI_ParseFrame(const uint8_t *frame, uint16_t len)
{
  if ((frame == NULL) || (len < 3u))
  {
    return;
  }

  if ((frame[0] == 0x65u) && (len >= 7u) && (s_touch_cb != NULL))
  {
    s_touch_cb(frame[1], frame[2], frame[3]);
  }
}

void HMI_Init(void)
{
  s_touch_cb = NULL;
  s_rx_head = 0u;
  s_rx_tail = 0u;
  s_rx_byte = 0u;
  (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1u);
}

void HMI_Task(void)
{
  uint8_t frame[64];
  uint16_t frame_len;

  while (HMI_RxReadFrame(frame, sizeof(frame), &frame_len))
  {
    HMI_ParseFrame(frame, frame_len);
  }
}
