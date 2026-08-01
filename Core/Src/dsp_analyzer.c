/**
  ******************************************************************************
  * @file           : dsp_analyzer.c
  * @brief          : DSP 频谱分析模块实现
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "dsp_analyzer.h"
#include <string.h>

/* 内存分配 (全局变量, 防止栈溢出) -----------------------------------------*/
__attribute__((aligned(4))) static float32_t fft_input_buf[FFT_LEN];
__attribute__((aligned(4))) static float32_t fft_output_cplx[FFT_LEN];
__attribute__((aligned(4))) static float32_t fft_mag_buffer[FFT_LEN / 2];

static arm_rfft_fast_instance_f32 rfft_instance;

/* 存储最终分析结果: [0]为基波, [1]为二次谐波, [2]为三次谐波... */
static SpectralComponent Signal_Spectrum[50];

/* 按 amp_mV 降序排列的前三名 */
Top3Spectrum Top3_Spectrum = {0};

/* 调试标记 */
static uint8_t k1234 = 0;

/* --------------------------------------------------------------------------*/
/*                         内部函数实现                                       */
/* --------------------------------------------------------------------------*/

/**
 * @brief 对指定峰值索引进行 Rife 插值, 提取高精度频率与幅值
 */
static SpectralComponent Extract_Peak_Info(uint32_t k0, float32_t *mag_buf)
{
    SpectralComponent comp = {0};
    comp.isValid = true;

    float32_t A0 = mag_buf[k0];
    float32_t A_left  = mag_buf[k0 - 1];
    float32_t A_right = mag_buf[k0 + 1];

    float32_t A1 = 0.0f;
    float32_t s  = 0.0f;

    /* 确定能量泄漏的主要方向 (寻找最大的相邻谱线) */
    if (A_right > A_left) {
        A1 = A_right;
        s  = 1.00f;
    } else {
        A1 = A_left;
        s  = -1.00f;
    }

    /* 计算绝对偏移量 delta_abs (0 <= delta_abs <= 0.5) */
    float32_t delta_abs = A1 / (A0 + A1);

    /* 计算频率 */
    float32_t delta = s * delta_abs;
    comp.freq_Hz = ((float32_t)k0 + delta) * FREQ_RES;

    /* 利用公式计算精确幅值 (Rife Amplitude Recovery) */
    float32_t true_amp;

    if (delta_abs < 0.001f) {
        /* 几乎正中频点, 直接归一化 */
        true_amp = (2.0f / (float32_t)FFT_LEN) * A0;
        k1234 = 1;
    } else {
        /* 数学补偿计算 */
        float32_t pi_d = PI * delta_abs;
        float32_t correction_factor = (pi_d * (1.0f - delta_abs)) / arm_sin_f32(pi_d);
        true_amp = (2.0f / (float32_t)FFT_LEN) * correction_factor * (A0 + A1);
    }

    comp.amp_mV = true_amp;
    return comp;
}

/**
 * @brief 全流程频域测量与分析
 * @param pRawSamples_mV 原始 ADC 数据 (需事先转换为 mV 单位传入)
 */
static void Perform_Full_Spectrum_Analysis(float32_t *pRawSamples_mV)
{
    uint32_t i;
    float32_t mean_val = 0.0f;
    SpectralComponent peak_info = {0};

    /* 1. 去除直流, 防止零频干扰 */
    arm_mean_f32(pRawSamples_mV, FFT_LEN, &mean_val);
    for (i = 0; i < FFT_LEN; i++) {
        fft_input_buf[i] = pRawSamples_mV[i] - mean_val;
    }

    /* 2. 执行实数 FFT */
    arm_rfft_fast_f32(&rfft_instance, fft_input_buf, fft_output_cplx, 0);

    /* 3. 求模值 */
    arm_cmplx_mag_f32(fft_output_cplx, fft_mag_buffer, FFT_LEN / 2);

    /* 4. 全局战略侦察: 找出整个频谱中的绝对山峰 (Global Maximum) */
    float32_t global_max_mag = 0.0f;
    uint32_t  global_max_idx = MIN_FREQ_INDEX;

    for (i = MIN_FREQ_INDEX; i <= MAX_FREQ_INDEX; i++) {
        if (fft_mag_buffer[i] > global_max_mag) {
            global_max_mag = fft_mag_buffer[i];
            global_max_idx = i;
        }
    }

    /* =====================================================================
     * 等效门限: 动态计算真实底噪 (Noise Floor) 与三倍门限
     * ===================================================================== */
    float32_t sum_mag = 0.0f;
    uint32_t valid_bins = MAX_FREQ_INDEX - MIN_FREQ_INDEX + 1;

    /* Pass 1: 求有效频段的全局粗略平均值 */
    for (i = MIN_FREQ_INDEX; i <= MAX_FREQ_INDEX; i++) {
        sum_mag += fft_mag_buffer[i];
    }
    float32_t mean_mag_pass1 = sum_mag / (float32_t)valid_bins;

    /* Pass 2: 剔除强信号波峰, 只对真实的背景噪声求均值 */
    float32_t noise_sum = 0.0f;
    uint32_t noise_bins_count = 0;

    for (i = MIN_FREQ_INDEX; i <= MAX_FREQ_INDEX; i++) {
        if (fft_mag_buffer[i] < mean_mag_pass1 * 10.0f) {
            noise_sum += fft_mag_buffer[i];
            noise_bins_count++;
        }
    }

    float32_t noise_floor = (noise_bins_count > 0) ? (noise_sum / (float32_t)noise_bins_count) : mean_mag_pass1;
    float32_t valid_thresh = noise_floor * 4.05f;

    /* =====================================================================
     * 第 5 步: 自左向右扫描, 锁定第一个显著极值点, 确认为基波 (Fundamental)
     * ===================================================================== */
    uint32_t base_k0 = 0;

    /* 清空最终结果状态 */
    for (i = 0; i < 5; i++) {
        Signal_Spectrum[i].isValid = false;
    }

    uint32_t search_radius = 10;
    uint32_t j;

    for (i = MIN_FREQ_INDEX; i <= MAX_FREQ_INDEX; i++) {
        /* 触发条件: 大于底噪门限, 且是局部的极值点 */
        if ((fft_mag_buffer[i] > valid_thresh) &&
            (fft_mag_buffer[i] > fft_mag_buffer[i - 1]) &&
            (fft_mag_buffer[i] > fft_mag_buffer[i + 1]))
        {
            search_radius = i / 4;
            uint32_t search_start = (i >= search_radius) ? (i - search_radius) : MIN_FREQ_INDEX;
            uint32_t search_end = (i + search_radius <= MAX_FREQ_INDEX) ? (i + search_radius) : MAX_FREQ_INDEX;

            float32_t local_max_mag = 0.0f;
            uint32_t  local_max_idx = i;

            /* 在动态窗口内寻找真正的最大极值点 */
            for (j = search_start; j <= search_end; j++) {
                if (fft_mag_buffer[j] > local_max_mag) {
                    local_max_mag = fft_mag_buffer[j];
                    local_max_idx = j;
                }
            }
            peak_info = Extract_Peak_Info(local_max_idx, fft_mag_buffer);
            if (peak_info.amp_mV > 7.0f) {
                Signal_Spectrum[0] = peak_info;
                base_k0 = local_max_idx;
                break;
            } else {
                continue;
            }
        }
    }

    if (base_k0 == 0) {
        return; /* 全局连基频都没有, 直接退出 */
    }

    /* =====================================================================
     * 第 6 步: 基于基频倍数的动态分区扫描法 (1.9x~2.1x, 2.9x~3.1x...)
     * ===================================================================== */
    for (uint32_t n = 2; n <= 50; n++) {
        /* 划定当前次谐波的绝对防区 */
        uint32_t zone_start = (uint32_t)((float32_t)base_k0 * ((float32_t)n - 0.1f));
        uint32_t zone_end   = (uint32_t)((float32_t)base_k0 * ((float32_t)n + 0.1f));

        /* 物理边界安全保护 */
        if (zone_start > MAX_FREQ_INDEX) {
            break;
        }
        if (zone_end > MAX_FREQ_INDEX) {
            zone_end = MAX_FREQ_INDEX;
        }

        /* 隔离带保护 */
        if (zone_start <= base_k0 + 5) {
            zone_start = base_k0 + 5;
        }

        float32_t zone_max_mag = 0.0f;
        uint32_t  zone_max_idx = zone_start;

        /* 在分区界限内寻找全局最大值 */
        for (j = zone_start; j <= zone_end; j++) {
            if (fft_mag_buffer[j] > zone_max_mag) {
                zone_max_mag = fft_mag_buffer[j];
                zone_max_idx = j;
            }
        }

        /* 最终判决: 是否真实谐波 */
        if ((zone_max_mag > valid_thresh) &&
            (fft_mag_buffer[zone_max_idx] > fft_mag_buffer[zone_max_idx - 1]) &&
            (fft_mag_buffer[zone_max_idx] > fft_mag_buffer[zone_max_idx + 1]))
        {
            Signal_Spectrum[n - 1] = Extract_Peak_Info(zone_max_idx, fft_mag_buffer);
        } else {
            Signal_Spectrum[n - 1].isValid = false;
        }
    }
}

/* --------------------------------------------------------------------------*/
/*                         对外接口函数                                       */
/* --------------------------------------------------------------------------*/

/**
 * @brief 初始化 FFT 分析器
 */
void DSP_Analyzer_Init(void)
{
    arm_rfft_fast_init_f32(&rfft_instance, FFT_LEN);
    memset(&Top3_Spectrum, 0, sizeof(Top3_Spectrum));
}

/**
 * @brief 全流程频谱分析接口
 *
 * 输入:  raw_adc_buf — 4096 点原始 ADC 计数值 (float32 数组)
 *        内部自动转换为 mV: value / 4095.0 * 3300.0
 *
 * 输出:  out_freq1/out_amp1 — Top1 频率 & 幅值
 *        out_freq2/out_amp2 — Top2 频率 & 幅值
 *        out_freq3/out_amp3 — Top3 频率 & 幅值
 */
void DSP_Analyzer_Process(float32_t *raw_adc_buf,
                          float32_t *out_freq1, float32_t *out_amp1,
                          float32_t *out_freq2, float32_t *out_amp2,
                          float32_t *out_freq3, float32_t *out_amp3)
{
    uint32_t i;

    DSP_Analyzer_Init();

    /* ---- 1. ADC 计数值 → mV 转换 (static, 避免 4KB 栈溢出) ---- */
    static float32_t samples_mV[FFT_LEN];
    for (i = 0; i < FFT_LEN; i++) {
        samples_mV[i] = raw_adc_buf[i] / ADC_MAX_COUNT * ADC_REF_MV;
    }

    /* ---- 2. 执行全频域分析 ---- */
    Perform_Full_Spectrum_Analysis(samples_mV);

    /* ---- 3. 筛选 Top3 按 amp_mV 降序排序 ---- */
    Top3_Spectrum.count = 0;
    for (uint32_t idx = 0; idx < 50; idx++) {
        if (!Signal_Spectrum[idx].isValid) continue;

        float32_t cur_amp = Signal_Spectrum[idx].amp_mV;
        uint8_t insert_pos = Top3_Spectrum.count;

        /* 在已有序列表中寻找插入位置 (降序) */
        for (uint8_t j = 0; j < Top3_Spectrum.count; j++) {
            if (cur_amp > Top3_Spectrum.components[j].amp_mV) {
                insert_pos = j;
                break;
            }
        }

        /* 后移腾出空位 (最多保留 3 个) */
        uint8_t shift_end = (Top3_Spectrum.count < 3) ? Top3_Spectrum.count : 2;
        for (uint8_t k = shift_end; k > insert_pos; k--) {
            Top3_Spectrum.components[k] = Top3_Spectrum.components[k - 1];
        }

        /* 插入当前元素 */
        if (insert_pos < 3) {
            Top3_Spectrum.components[insert_pos] = Signal_Spectrum[idx];
        }

        if (Top3_Spectrum.count < 3) {
            Top3_Spectrum.count++;
        }
    }

    /* ---- 4. 填充六个输出接口 ---- */
    if (Top3_Spectrum.count > 0) {
        *out_freq1 = Top3_Spectrum.components[0].freq_Hz;
        *out_amp1  = Top3_Spectrum.components[0].amp_mV;
    } else {
        *out_freq1 = 0.0f;
        *out_amp1  = 0.0f;
    }

    if (Top3_Spectrum.count > 1) {
        *out_freq2 = Top3_Spectrum.components[1].freq_Hz;
        *out_amp2  = Top3_Spectrum.components[1].amp_mV;
    } else {
        *out_freq2 = 0.0f;
        *out_amp2  = 0.0f;
    }

    if (Top3_Spectrum.count > 2) {
        *out_freq3 = Top3_Spectrum.components[2].freq_Hz;
        *out_amp3  = Top3_Spectrum.components[2].amp_mV;
    } else {
        *out_freq3 = 0.0f;
        *out_amp3  = 0.0f;
    }
}

/**
 * @brief 导出 FFT 幅度谱 (截取前 SPECTRUM_MAX_POINTS 点, 转 uint16)
 */
void DSP_Analyzer_GetSpectrum(uint16_t *out, uint16_t *out_count)
{
    uint16_t n;
    uint16_t i;

    if ((out == NULL) || (out_count == NULL)) {
        return;
    }

    n = (uint16_t)(FFT_LEN / 2u);
    if (n > SPECTRUM_MAX_POINTS) {
        n = SPECTRUM_MAX_POINTS;
    }

    for (i = 0u; i < n; i++) {
        float32_t v = fft_mag_buffer[i];
        if (v < 0.0f) {
            v = 0.0f;
        }
        if (v > 65535.0f) {
            v = 65535.0f;
        }
        out[i] = (uint16_t)v;
    }

    *out_count = n;
}
