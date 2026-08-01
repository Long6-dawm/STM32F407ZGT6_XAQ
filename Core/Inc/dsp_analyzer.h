/**
  ******************************************************************************
  * @file           : dsp_analyzer.h
  * @brief          : DSP 频谱分析模块接口
  ******************************************************************************
  */

#ifndef __DSP_ANALYZER_H__
#define __DSP_ANALYZER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "arm_math.h"
#include <stdbool.h>
#include <stdint.h>

/* 常量定义 ----------------------------------------------------------------*/
#define FFT_LEN           1024
#define SAMPLE_RATE       1000000.0f
#define FREQ_RES          (SAMPLE_RATE / FFT_LEN)         /* 500 Hz */

#define MIN_FREQ_INDEX    10                              /* 10 kHz */
#define MAX_FREQ_INDEX    512                            /* 500 kHz */

#define ADC_MAX_COUNT     1024.0f
#define ADC_REF_MV        3300.0f

/* 类型定义 ----------------------------------------------------------------*/

/** 频谱分量结构体 */
typedef struct {
    bool      isValid;       /* 该分量是否有效 */
    float32_t freq_Hz;       /* 精确频率 (Hz) */
    float32_t amp_mV;        /* 精确电压幅值 (mV) */
} SpectralComponent;

/** 幅值最大的前三个有效谱线 */
typedef struct {
    SpectralComponent components[3];  /* 按 amp_mV 降序排列 */
    uint8_t           count;          /* 实际有效条目数 (0~3) */
} Top3Spectrum;

/* 全局变量声明 ------------------------------------------------------------*/
extern Top3Spectrum Top3_Spectrum;

/* 函数声明 ----------------------------------------------------------------*/

/**
 * @brief  初始化 FFT 分析器 (必须先调用一次)
 */
void DSP_Analyzer_Init(void);

/**
 * @brief  全流程频谱分析接口函数
 * @param  raw_adc_buf  原始 ADC 采样数据 (4096 点 float32 数组), 单位为 ADC 计数值
 * @param  out_freq1    输出: Top1 频率 (Hz)
 * @param  out_amp1     输出: Top1 幅值 (mV)
 * @param  out_freq2    输出: Top2 频率 (Hz)
 * @param  out_amp2     输出: Top2 幅值 (mV)
 * @param  out_freq3    输出: Top3 频率 (Hz)
 * @param  out_amp3     输出: Top3 幅值 (mV)
 * @note   内部自动完成 ADC 计数值 → mV 的转换 (value / 4095 * 3300)
 *         若有效分量不足 3 个, 对应输出为 0
 */
void DSP_Analyzer_Process(float32_t *raw_adc_buf,
                          float32_t *out_freq1, float32_t *out_amp1,
                          float32_t *out_freq2, float32_t *out_amp2,
                          float32_t *out_freq3, float32_t *out_amp3);

#ifdef __cplusplus
}
#endif

#endif /* __DSP_ANALYZER_H__ */
