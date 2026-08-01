/**
 * @file hmi_tjc.h
 * @brief TJC USART HMI command helper.
 */
#ifndef HMI_TJC_H
#define HMI_TJC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*HMI_OnTouchCb)(uint8_t page_id, uint8_t component_id, uint8_t event_type);

void HMI_Init(void);
void HMI_Task(void);

void HMI_SendCmd(const char *cmd);
void HMI_SetText(const char *obj, const char *text);
void HMI_SetValue(const char *obj, int32_t value);
void HMI_SetColor565(const char *obj, uint16_t color);
void HMI_AddWavePoint(const char *obj, uint8_t channel, uint8_t value);
void HMI_Addt_Send(const char *curve_id, uint8_t ch, const uint8_t *data, uint16_t len);
void HMI_ClearWave(const char *obj, uint8_t channel_mask);
void HMI_GotoPage(const char *page_name);
void HMI_SetTouchCallback(HMI_OnTouchCb cb);

int __io_putchar(int ch);

#ifdef __cplusplus
}
#endif

#endif /* HMI_TJC_H */
