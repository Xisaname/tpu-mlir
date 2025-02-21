//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"
#include "float.h"
#include "mlir/IR/PatternMatch.h"
#include "omp.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"
#include "llvm/Support/Debug.h"
#include <algorithm>
#include <map>
#include <numeric>
#include <queue>

#define DEBUG_TYPE "math_utils"
namespace tpu_mlir {

void get_scale_and_shift(float scale_f, int &scale, int &shift, int bitwidth) {
  float min_err = FLT_MAX;
  int m_limit = (bitwidth == 32) ? INT_MAX : CHAR_MAX;
  for (int n = -32; n < 31; n++) {
    // 若scale_f大于等于1，这里循环上限要设为31(而不是32)，且越大则需减少越多，暂只考虑scale_f小于等于1的情形
    //  wxc 20220119
    int m = (int)std::round(scale_f * std::pow(2, n));
    float err = std::abs(m / std::pow(2, n) - scale_f);
    if (err < min_err && abs(m) < m_limit) {
      min_err = err;
      shift = n;
    }
  }
  scale = (int)std::round(scale_f * std::pow(2, shift));
}

void get_scale_and_shift_positive(float scale_f, int &scale, int &shift,
                                  int bitwidth) {
  float min_err = FLT_MAX;
  int m_limit = (bitwidth == 32) ? INT_MAX : CHAR_MAX;
  for (int n = 0; n < 31; n++) {
    int m = (int)std::round(scale_f * std::pow(2, n));
    float err = std::abs(m / std::pow(2, n) - scale_f);
    if (err < min_err && abs(m) < m_limit) {
      min_err = err;
      shift = n;
    }
  }
  scale = (int)std::round(scale_f * std::pow(2, shift));
}

// this function search positive right shift with max_shift, max_shift set to 8
// for int16 op and shift to 8bit output.
void get_scale_and_shift_positive_maxshift(float scale_f, int &scale,
                                           int &shift, int bitwidth,
                                           int max_shift) {
  float min_err = FLT_MAX;
  int m_limit = (bitwidth == 32) ? INT_MAX : CHAR_MAX;
  for (int n = 0; n < max_shift; n++) {
    int m = (int)std::round(scale_f * std::pow(2, n));
    float err = std::abs(m / std::pow(2, n) - scale_f);
    if (err < min_err && abs(m) < m_limit) {
      min_err = err;
      shift = n;
    }
  }
  scale = (int)std::round(scale_f * std::pow(2, shift));
}

template <typename Dtype> float findMaxabs(const Dtype *pSrcData, int len) {
  float fmax = 0.0;
  float dataTmp;
  for (int i = 0; i < len; i++) {
    dataTmp = (float)pSrcData[i];
    fmax = (fabs(dataTmp) > fmax) ? fabs(dataTmp) : fmax;
  }
  if (fmax == 0.0) {
    ; // LOG(WARNING) << "findMaxabs meet fmax == 0";
  }

  return fmax;
}
template float findMaxabs<float>(const float *pSrcData, int len);
template float findMaxabs<int>(const int *pSrcData, int len);

template <typename Dtype>
void findMinMax(const Dtype *pSrcData, int len, Dtype *minVal, Dtype *maxVal) {
  Dtype fmin = pSrcData[0];
  Dtype fmax = pSrcData[0];
  Dtype dataTmp;
  for (int i = 0; i < len; i++) {
    dataTmp = pSrcData[i];
    fmin = dataTmp < fmin ? dataTmp : fmin;
    fmax = dataTmp > fmax ? dataTmp : fmax;
  }
  *minVal = fmin;
  *maxVal = fmax;
}

template void findMinMax<float>(const float *pSrcData, int len, float *minVal,
                                float *maxVal);
template void findMinMax<int>(const int *pSrcData, int len, int *minVal,
                              int *maxVal);

int calRightShiftNum(float fmax, double thBottom, double thTop, int numBits) {
  double dataTmp = 1.0 * fmax * thBottom / thTop;
  int m = 0;

  if (dataTmp <= 0.0) {
    llvm::errs() << "meet dataTmp <= 0.0, fmax:" << fmax
                 << " thBottom:" << thBottom << " thTop:" << thTop;
    return 0;
  }
  while (dataTmp < ((1 << (numBits - 1)) - 1)) {
    dataTmp = dataTmp * 2;
    m = m + 1;
  }

  m = m > 32 ? 31 : m - 1;
  return m;
}

template <typename T> void func_abs(int n, T *src, T *dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = std::abs(src[i]);
  }
}

template <typename T> void func_log(int n, T *src, T *dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = std::log(src[i]);
  }
}

int calRightShiftNumUseCblas(float fmax, double thBottom, double thTop,
                             int numBits) {
  func_abs(1, &fmax, &fmax);
  double dataTmp = 1.0 * ((1 << (numBits - 1)) - 1) / (fmax * thBottom / thTop);
  int m = 0;

  double log_dem, log_num;
  double data2 = 2.0;

  func_log(1, &dataTmp, &log_dem);
  func_log(1, &data2, &log_num);

  m = floor(log_dem / log_num);
  m = m > 31 ? 31 : m;

  return m;
}

float func_log2(double dataInput) {
  double log_dem, log_num;
  double data2 = 2.0;
  float result;

  func_log(1, &dataInput, &log_dem);
  func_log(1, &data2, &log_num);

  result = log_dem / log_num;

  return result;
}

void quantizeToInt32(const float *pSrc, int32_t *pDst, int len, float scale) {
  // used in CV18xx bias quant
  int32_t qmax = INT_MAX;
  int32_t qmin = INT_MIN;
  int64_t tmp = 0;
  for (int i = 0; i < len; i++) {
    tmp = to_int(pSrc[i] * scale, ROUNDING_HALF_TO_EVEN);
    pDst[i] = (int32_t)((tmp > qmax) ? qmax : ((tmp < qmin) ? qmin : tmp));
  }
}

float quantizeToInt16(const float *pSrc, int16_t *pDst, int len, float scale,
                      int rshift) {
  int16_t qmax = 32767;
  int16_t qmin = -32768;
  int tmp = 0;
  int overflow = 0;
  int half_data = (rshift == 0) ? 0 : 1 << (rshift - 1);

  for (int i = 0; i < len; i++) {
    tmp = round(pSrc[i] * scale);
    tmp = (tmp + half_data) >> rshift;
    pDst[i] = (int16_t)((tmp > qmax) ? qmax : ((tmp < qmin) ? qmin : tmp));
    overflow = (tmp > qmax || tmp < qmin) ? overflow + 1 : overflow;
  }
  float ratio = ((float)overflow) / ((float)len);
  if (ratio > 0 && len > 1) {
    llvm::errs() << "ratio of overflow = " << ratio;
  }
  return ratio;
}

float quantizeToInt15(const float *pSrc, int16_t *pDst, int len, float scale,
                      int rshift) {
  int16_t qmax = 16383;
  int16_t qmin = -16384;
  int tmp = 0;
  int overflow = 0;
  int half_data = (rshift == 0) ? 0 : 1 << (rshift - 1);

  for (int i = 0; i < len; i++) {
    tmp = floor(pSrc[i] * scale + 0.5);
    tmp = (tmp + half_data) >> rshift;
    pDst[i] = (int16_t)((tmp > qmax) ? qmax : ((tmp < qmin) ? qmin : tmp));
    overflow = (tmp > qmax || tmp < qmin) ? overflow + 1 : overflow;
  }
  float ratio = ((float)overflow) / ((float)len);
  if (ratio > 0) {
    llvm::errs() << "ratio of overflow = " << ratio;
  }
  return ratio;
}

template <typename T>
void quantizeToInt8(const float *pSrc, T *pDst, int len, float scale,
                    RoundingMode round_mode) {
  for (int i = 0; i < len; i++) {
    pDst[i] = to_int8(pSrc[i] * scale, round_mode);
  }
}

template void quantizeToInt8(const float *pSrc, int8_t *pDst, int len,
                             float scale, RoundingMode round_mode);
template void quantizeToInt8(const float *pSrc, float *pDst, int len,
                             float scale, RoundingMode round_mode);

// tensorflow/lite/kernels/internal/quantization_util.cc
// mlir/lib/Dialect/Tosa/Utils/QuantUtils.cpp
// to compitable with tflite
void QuantizeMultiplier(double double_multiplier, int64_t *quantized_multiplier,
                        int64_t *shift) {
  if (double_multiplier == 0.) {
    *quantized_multiplier = 0;
    *shift = 0;
    return;
  }
  int shift_tmp;
  const double q = std::frexp(double_multiplier, &shift_tmp);
  *shift = shift_tmp;
  auto q_fixed = static_cast<int64_t>(std::round(q * (1LL << 31)));
  assert(q_fixed <= (1LL << 31));
  if (q_fixed == (1LL << 31)) {
    q_fixed /= 2;
    ++*shift;
  }
  assert(q_fixed <= std::numeric_limits<int32_t>::max());
  // A shift amount smaller than -31 would cause all bits to be shifted out
  // and thus all results would be zero. We implement that instead with
  // q_fixed==0, so as to avoid hitting issues with right-shift
  // operations with shift amounts greater than 31. Note that this happens
  // roughly when abs(double_multiplier) < 2^-31 and the present handling means
  // that we're effectively flushing tiny double_multiplier's to zero.
  // We could conceivably handle values in the range (roughly) [32, 63]
  // as 'denormals' i.e. (shift==0, q_fixed < 2^30). In that point of view
  // the present handling is just doing 'flush denormals to zero'. We could
  // reconsider and actually generate nonzero denormals if a need arises.
  if (*shift < -31) {
    *shift = 0;
    q_fixed = 0;
  }
  // Single-rounding MultiplyByQuantizedMultiplier doesn't support a shift > 30,
  // saturate it.
  if (*shift > 30) {
    *shift = 30;
    q_fixed = (1LL << 31) - 1;
  }
  // Sophgo expects right shift to be positive, and embed (1 << 31) into right
  // shift bits.
  // *shift = (-*shift) + 31;
  *quantized_multiplier = static_cast<int32_t>(q_fixed);
}

// CV18xx
double getQscaleForFilter(float max_filter, float threshold_y,
                          float threshold_x, int quant_bitwidth) {
  /// get a QScale for Filter (with multiplier)
  ///   Q(W) = W * (threshold_x / threshold_y) * (1 / QScale)
  ///   find a QScale so that Q(max_filter) = 127
  /// used in CV18xx per-channel mode
  /// During runtime
  ///   HW multiples the accumulated result by QScale before saturate to Int8
  ///   QScale is then decomposed into a multipler and a rshift
  ///   => QScale = Multiplier / (1 << RShift)
  ///   where Multiplier is an interger
  if (threshold_y <= 0) {
    llvm::errs() << "WARNING: findQScaleForFilter threshold_y = " << threshold_y
                 << "\n";
    threshold_y = 0.00001;
  }
  float max_quant = (float)((1 << (quant_bitwidth - 1)) - 1); // 127
  double qscale = (max_filter * threshold_x) / (max_quant * threshold_y);
  return qscale;
}

double getQscaleForBias(float max_bias, float threshold_y) {
  /// get a QScale For Bias I32
  ///   Q(B) = B * (127.0f / threshold_y)  * (1 / QScale)
  ///   find a QScale so that Q(max_bias) = 0x7fffffff
  /// used in CV18xx per-channel mode
  /// During runtime
  ///   HW multiples the accumulated result by QScale before saturate to Int8
  ///   QScale is then decomposed into a multipler and a rshift
  ///   => QScale = Multiplier / (1 << RShift)
  ///   where Multiplier is an interger
  if (threshold_y <= 0) {
    llvm::errs() << "WARNING: findQScaleForBiasI32 threshold_y = "
                 << threshold_y << "\n";
    threshold_y = 0.00001;
  }
  double qscale = (max_bias * 127.0f) / (0x7fffffff * 0.998 * threshold_y);
  return qscale;
}

void getRShiftAndMultiplierFromQScale(double double_multiplier,
                                      int64_t *multiplier, int64_t *shift,
                                      bool qdm, int64_t max_multiplier) {
  /// find RShift and Multiplier from QScale
  ///   QScale = Multiplier / (1 << RShift)
  ///   Multiplier is an integer
  /// case 1: specifically multiply a int8/uint8 multplier, then rshift
  ///   used in layers like element_wise, pooling, concat, etc
  ///   qdm is false
  ///   a max_multiplier (127 or 255 normally) has to be provided
  /// case 2: qdm mode
  ///   used in CV18xx per-channel conv mode
  ///   qdm is true
  ///   reference to [arxiv 1712.05877]
  ///     choose the int32 value nearest to 2^31 * M0, M0 in [0.5, 1]
  ///     this value is always at least 2^30 and have at least 30 bits accuracy
  ///   the max_multiplier argument is ignored, fixed to (1 << 31)
  /// if 'uint32_t *multiplier' is present, return multipler alongside
  int64_t quantized_multiplier = 0;
  if (qdm) {
    if (double_multiplier >= 1) {
      double_multiplier = 0.999999;
      llvm::errs() << "WARNING: qscale > 1,  = " << double_multiplier << "\n";
    }
    QuantizeMultiplier(double_multiplier, &quantized_multiplier, shift);
    *shift = -*shift;
    LLVM_DEBUG(if (*shift > 25) {
      llvm::errs() << "WARNING: large rshift = " << *shift
                   << ", qscale = " << double_multiplier << "\n";
    });
  } else {
    if (double_multiplier > max_multiplier) {
      llvm::errs() << "WARNING: qscale > max_multipiler ( " << double_multiplier
                   << " v.s. " << max_multiplier << " )\n";
      quantized_multiplier = max_multiplier;
      *shift = 0;
    } else {
      bool found = false;
      for (int64_t rshift = 0; rshift < 63; ++rshift) {
        if (double_multiplier * (1ULL << (rshift + 1)) >=
            (double)max_multiplier) {
          found = true;
          quantized_multiplier =
              (int64_t)(double_multiplier * (1ULL << rshift));
          *shift = rshift;
          break;
        }
      }
      if (!found) {
        // we are here because qscale is too small, return 0 for both shift and
        // multiplier
        LLVM_DEBUG(llvm::errs() << "WARNING: failed to find rshift, qscale = "
                                << std::to_string(double_multiplier) << "\n";);
        quantized_multiplier = 0;
        *shift = 0;
      }
    }
  }
  if (multiplier) {
    *multiplier = quantized_multiplier;
  }
}

int8_t getMultiplierI8FromQScaleAndRShift(double qscale, int8_t rshift) {
  return (uint32_t)(qscale * (1 << rshift));
}

int8_t quantizeFilterRShift(float w, float threshold_y, float threshold_x,
                            uint32_t rshift) {
  double factor = (threshold_x / threshold_y) * (1 << rshift);
  float q_f = (float)(w * factor);
  return to_int8(q_f, ROUNDING_HALF_UP);
}

void quantizeFilterRShiftAndMultiplier(const float *pSrc, int8_t *pDst, int len,
                                       float threshold_y, float threshold_x,
                                       int64_t rshift, int64_t multiplier,
                                       bool qdm) {
  /// quantize a filter weight value into int8 based on rshift and multiplier
  ///   Q(W) = W * (threshold_x / threshold_y) * (1 / QScale)
  ///   QScale = Multiplier / (1 << RShift)
  /// used in CV18xx legacy per-layer mode
  if (qdm) {
    rshift += 31;
  }
  double factor = (multiplier == 0) ? 0
                                    : (double)(threshold_x / threshold_y) *
                                          (1ULL << rshift) / multiplier;
  quantizeToInt8(pSrc, pDst, len, factor, ROUNDING_HALF_TO_EVEN);
}

void quantizeBiasRShiftAndMultiplier(const float *pSrc, int32_t *pDst, int len,
                                     float threshold_y, int64_t rshift,
                                     int64_t multiplier, bool qdm) {
  /// quantize a bias weight value into int32 based on rshift and multiplier
  ///   Q(B) = B * (127.0f / threshold_y) * (1 / QScale)
  ///   QScale = Multiplier * (1 << RShift)
  /// used in CV18xx per-channel mode (32bit bias)
  if (qdm) {
    rshift += 31;
  }
  double factor = (multiplier == 0) ? 0
                                    : (double)(127.0f / threshold_y) *
                                          (1ULL << rshift) / multiplier;
  quantizeToInt32(pSrc, pDst, len, factor);
}

template <typename T>
T RightShiftRound(T src, int shift_num, RoundingMode round_mode) {
  if (shift_num == 0)
    return src;
  if (shift_num > 63)
    shift_num = 63;
  T val, res;
  if (shift_num < 0) {
    return src << (-shift_num);
  }
  val = src >> shift_num;
  res = val;
  T lo_mask = (1ull << shift_num) - 1;
  T mant = src & lo_mask;
  T mant_0d5 = 1ull << (shift_num - 1);
  if (round_mode == ROUNDING_HALF_TO_EVEN) {
    if (mant == mant_0d5)
      res = val + (val & 1);
    else if (mant > mant_0d5)
      res = val + 1;
  } else if (round_mode == ROUNDING_HALF_AWAY_FROM_ZERO) {
    if (src >= 0 && mant >= mant_0d5)
      res = val + 1;
    else if (src < 0 && mant > mant_0d5)
      res = val + 1;
  } else if (round_mode == ROUNDING_TOWARDS_ZERO) {
    if (src < 0)
      res = val + (mant != 0);
  } else if (round_mode == ROUNDING_DOWN)
    res = val;
  else if (round_mode == ROUNDING_UP)
    res = val + (mant != 0);
  else if (round_mode == ROUNDING_HALF_UP) {
    if (mant >= mant_0d5)
      res = val + 1;
  } else if (round_mode == ROUNDING_HALF_DOWN) {
    if (mant > mant_0d5)
      res = val + 1;
  }
  return res;
}

template long long RightShiftRound(long long src, int shift_num,
                                   RoundingMode round_mode);
template int64_t RightShiftRound(int64_t src, int shift_num,
                                 RoundingMode round_mode);

// to compilable with tflite
// tensorflow/lite/kernels/internal/common.h:MultiplyByQuantizedMultiplier()
int32_t MultiplyByQuantizedMultiplier(int32_t x, int32_t multiplier,
                                      int shift) {
  // int shift = -(rshift - 31);
  int64_t value = shift > 0 ? x << shift : x;
  value = RightShiftRound(value * multiplier, 31, ROUNDING_HALF_UP);
  if (value > (1ll << 31) - 1)
    value = (1ll << 31) - 1;
  else if (value < -(1ll << 31))
    value = -(1ll << 31);
  if (shift < 0) {
    value = RightShiftRound(value, -shift, ROUNDING_HALF_AWAY_FROM_ZERO);
  }
  return (int32_t)value;
}

int64_t applyMultiplierAndRShift(int64_t v, int64_t multiplier, int64_t rshift,
                                 tpu::RequantMode qmode) {
  switch (qmode) {
  case tpu::RequantMode::MultiplierShift:
    if (module::isCV18xx()) {
      return to_int(((((float)v * multiplier)) / (1 << rshift)),
                    ROUNDING_HALF_UP);
    } else {
      return RightShiftRound(v * multiplier, (int)rshift, ROUNDING_HALF_UP);
    }
  case tpu::RequantMode::OnlyShift:
    return RightShiftRound(v, (int)rshift, ROUNDING_HALF_UP);
  case tpu::RequantMode::QDM:
  case tpu::RequantMode::TFLite:
  case tpu::RequantMode::TFLite_LShift:
    if (module::isCV18xx()) {
      rshift = -rshift;
    }
    return MultiplyByQuantizedMultiplier((int32_t)v, (int32_t)multiplier,
                                         (int32_t)rshift);
  }
  llvm_unreachable("unsupport quant multiplier mode.");
  return 0;
}

void pad_tensor(float *p_after_pad, float *src, int n, int c, int h, int w,
                int pt, int pb, int pl, int pr, float pad_value) {
  int nc = n * c;
  int oh = h + pt + pb;
  int ow = w + pl + pr;
  for (int i = 0; i < nc; i++) {
    for (int j = 0; j < oh; j++) {
      for (int k = 0; k < ow; k++) {
        int d_offset = (i * oh + j) * ow + k;
        if (j < pt || j >= (pt + h) || k < pl || k >= (pl + w)) {
          p_after_pad[d_offset] = pad_value;
        } else {
          int s_offset = (i * h + j - pt) * w + k - pl;
          p_after_pad[d_offset] = src[s_offset];
        }
      }
    }
  }
}

void pad_tensor(float *p_after_pad, float *src, int n, int c, int d, int h,
                int w, int pdf, int pdb, int pht, int phb, int pwl, int pwr,
                float pad_value) {
  int nc = n * c;
  int od = d + pdf + pdb;
  int oh = h + pht + phb;
  int ow = w + pwl + pwr;
  for (int i = 0; i < nc; i++) {
    for (int m = 0; m < od; m++) {
      for (int j = 0; j < oh; j++) {
        for (int k = 0; k < ow; k++) {
          int d_offset = (i * od * oh + m * oh + j) * ow + k;
          if (m < pdf || m >= (pdf + d) || j < pht || j >= (pht + h) ||
              k < pwl || k >= (pwl + w)) {
            p_after_pad[d_offset] = pad_value;
          } else {
            int s_offset = ((i * d + m - pdf) * h + j - pht) * w + k - pwl;
            p_after_pad[d_offset] = src[s_offset];
          }
        }
      }
    }
  }
}

void pad_tensor_for_deconv(float *p_after_pad, float *src, int n, int c, int d,
                           int h, int w, int kd, int kh, int kw, int dd, int dh,
                           int dw, int sd, int sh, int sw, int pdf, int pdb,
                           int pht, int phb, int pwl, int pwr,
                           float pad_value) {
  int nc = n * c;
  int od = (d - 1) * sd + 1 + dd * (2 * kd - 2 - pdf - pdb);
  int oh = (h - 1) * sh + 1 + dh * (2 * kh - 2 - pht - phb);
  int ow = (w - 1) * sw + 1 + dw * (2 * kw - 2 - pwl - pwr);
  int pst[3] = {(kd - 1) * dd - pdf, (kh - 1) * dh - pht, (kw - 1) * dw - pwl};
  int ped[3] = {(kd - 1) * dd - pdb, (kh - 1) * dh - phb, (kw - 1) * dw - pwr};
  for (int i = 0; i < nc; i++) {
    for (int m = 0; m < od; m++) {
      for (int j = 0; j < oh; j++) {
        for (int k = 0; k < ow; k++) {
          int d_offset = (i * od * oh + m * oh + j) * ow + k;
          if (m < pst[0] || m >= (od - ped[0]) || j < pst[1] ||
              j >= (oh - ped[1]) || k < pst[2] || k >= (ow - ped[2]) ||
              (m - pst[0]) % sd != 0 || (j - pst[1]) % sh != 0 ||
              (k - pst[2]) % sw != 0) {
            p_after_pad[d_offset] = pad_value;
          } else {
            int s_offset =
                ((i * d + (m - pst[0]) / sd) * h + (j - pst[1]) / sh) * w +
                (k - pst[2]) / sw;
            p_after_pad[d_offset] = src[s_offset];
          }
        }
      }
    }
  }
}

void tensor_sub_zp(float *tensor_after_zp, float *src, int64_t length,
                   float zero_point) {
#pragma omp parallel for schedule(static, omp_schedule(length))
  for (int i = 0; i < length; ++i) {
    tensor_after_zp[i] = src[i] - zero_point;
  }
}

void tensor_hw_transpose(float *dst, float *src, int64_t N, int64_t C,
                         int64_t H, int64_t W) {
#pragma omp parallel for schedule(static, omp_schedule(N *C))
  for (int64_t nc = 0; nc < N * C; ++nc) {
    int64_t nc_offset = nc * H * W;
    for (int w = 0; w < W; ++w) {
      for (int h = 0; h < H; ++h) {
        int64_t d_offset = nc_offset + w * H + h;
        int64_t s_offset = nc_offset + h * W + w;
        dst[d_offset] = src[s_offset];
      }
    }
  }
}

void tensor_split(float *src_data, std::vector<std::vector<float>> &dst_data,
                  std::vector<int64_t> &shape, int slice_num, int axis) {
  assert(shape[axis] % slice_num == 0);
  assert(axis < shape.size());
  dst_data.resize(slice_num);

  // The data can be treated as 3 dim
  // 1.pre of the axis
  // 2.the axis
  // 3.behind of axis
  std::vector<int64_t> fake_shape(3);
  fake_shape[0] = std::accumulate(shape.begin(), shape.begin() + axis, 1,
                                  std::multiplies<int64_t>());
  fake_shape[1] = shape[axis];
  fake_shape[2] = std::accumulate(shape.begin() + axis + 1, shape.end(), 1,
                                  std::multiplies<int64_t>());
  std::vector<int64_t> fake_offset(3);
  fake_offset[2] = 1;
  fake_offset[1] = fake_offset[2] * fake_shape[2];
  fake_offset[0] = fake_offset[1] * fake_shape[1];

  int64_t indices = shape[1] / slice_num;
  int64_t slice_size = fake_shape[0] * indices * fake_offset[1];

  // each slice
#pragma omp parallel for schedule(static, omp_schedule(slice_num))
  for (int64_t i = 0; i < slice_num; ++i) {
    dst_data[i].resize(slice_size);
    // each fake dim 0
#pragma omp parallel for schedule(static, omp_schedule(fake_shape[0]))
    for (int64_t j = 0; j < fake_shape[0]; ++j) {
      float *src_ptr =
          src_data + j * fake_offset[0] + i * indices * fake_offset[1];
      float *dst_ptr = dst_data[i].data() + j * indices * fake_offset[1];
      std::copy(src_ptr, src_ptr + indices * fake_offset[1], dst_ptr);
    }
  }
}

int omp_schedule(int count) {
  return (count + omp_get_num_threads() - 1) / omp_get_num_threads();
}

void function_relu(float *src, float *dst, int64_t size, float relu_limit,
                   mlir::Type elem_type) {
#pragma omp parallel for schedule(static, omp_schedule(size))
  for (int64_t i = 0; i < size; ++i) {
    dst[i] = src[i] > 0 ? src[i] : 0;
    if (relu_limit > 0.f && dst[i] > relu_limit) {
      dst[i] = relu_limit;
    }
    if (elem_type && elem_type.isa<mlir::IntegerType>()) {
      dst[i] = saturate(dst[i], elem_type);
    }
  }
}

int dnnl_mm(float *input, float *weight, float *bias, float *output, int m,
            int k, int n, bool transpose) {
  if (!bias) {
    auto zero_bias = new std::vector<float>(n, 0.0f);
    bias = zero_bias->data();
  }

#ifdef DUMP_FLAG
  static int dump_idx = 0;
  std::string prefix = std::string("ip") + std::to_string(dump_idx);
  if (dump_idx == 0) {
    write_bianry_file(prefix + std::string("_in.bin"), (const char *)input,
                      m * k * sizeof(float));
  }
#endif // DUMP_FLAG

  using tag = memory::format_tag;
  using dt = memory::data_type;

  engine eng(engine::kind::cpu, 0);
  stream s(eng);

  std::vector<primitive> net;
  std::vector<std::unordered_map<int, memory>> net_args;

  memory::dims src_tz = {m, k};
  memory::dims weights_tz = {n, k};
  memory::dims bias_tz = {n};
  memory::dims dst_tz = {m, n};

  if (!bias) {
    auto zero_bias = new std::vector<float>(n, 0.0f);
    bias = zero_bias->data();
  }

  // memory
  auto user_src_memory = memory({{src_tz}, dt::f32, tag::nc}, eng, input);
  auto user_weights_memory =
      memory({{weights_tz}, dt::f32, tag::oi}, eng, weight);
  auto user_bias_memory = memory({{bias_tz}, dt::f32, tag::x}, eng, bias);
  auto user_dst_memory = memory({{dst_tz}, dt::f32, tag::nc}, eng, output);

  // md
  auto src_md = memory::desc({src_tz}, dt::f32, tag::any);
  auto weights_md = memory::desc({weights_tz}, dt::f32, tag::any);
  auto bias_md = memory::desc({bias_tz}, dt::f32, tag::any);
  auto dst_md = memory::desc({dst_tz}, dt::f32, tag::any);

  // fc desc
  auto fc_desc = inner_product_forward::desc(
      prop_kind::forward_inference, src_md, weights_md, bias_md, dst_md);
  auto fc_prim_desc = inner_product_forward::primitive_desc(fc_desc, eng);

  // do reorder if needed
  auto src_memory = user_src_memory;
  if (fc_prim_desc.src_desc() != user_src_memory.get_desc()) {
    src_memory = memory(fc_prim_desc.src_desc(), eng);
    net.push_back(reorder(user_src_memory, src_memory));
    net_args.push_back(
        {{DNNL_ARG_FROM, user_src_memory}, {DNNL_ARG_TO, src_memory}});
  }
  auto weights_memory = user_weights_memory;
  if (fc_prim_desc.weights_desc() != user_weights_memory.get_desc()) {
    weights_memory = memory(fc_prim_desc.weights_desc(), eng);
    reorder(user_weights_memory, weights_memory)
        .execute(s, user_weights_memory, weights_memory);
  }
  auto bias_memory = user_bias_memory;

  auto dst_memory = memory(fc_prim_desc.dst_desc(), eng);

  net.push_back(inner_product_forward(fc_prim_desc));
  net_args.push_back({{DNNL_ARG_SRC, src_memory},
                      {DNNL_ARG_WEIGHTS, weights_memory},
                      {DNNL_ARG_BIAS, bias_memory},
                      {DNNL_ARG_DST, dst_memory}});

  // reorder or copy the output
  if (dst_memory != user_dst_memory) {
    net.push_back(reorder(dst_memory, user_dst_memory));
    net_args.push_back(
        {{DNNL_ARG_FROM, dst_memory}, {DNNL_ARG_TO, user_dst_memory}});
  }

  // run
  assert(net.size() == net_args.size() && "something is missing");
  for (size_t i = 0; i < net.size(); ++i)
    net.at(i).execute(s, net_args.at(i));

  s.wait();

#ifdef DUMP_FLAG
  if (dump_idx == 0) {
    write_bianry_file(prefix + std::string("_out.bin"), (const char *)output,
                      m * n * sizeof(float));
  }
  dump_idx++;
#endif // DUMP_FLAG

  return 0;
}

void stride_slice_gen_params(const int64_t *input_shape_, int input_dim_,
                             const float *begin_index_, const float *end_index_,
                             const float *strides_, int strides_size,
                             int begin_mask_, int end_mask_, int ellipsis_mask_,
                             int new_axis_mask_, int shrink_axis_mask_,
                             int *input_shape, int *input_dim, int *begin_index,
                             int *end_index, int *strides, int *begin_mask,
                             int *end_mask, int *shrink_axis_mask) {
  int sdim = strides_size;
  bool ellipsis_seen = false;
  int num_add_axis_after_ellipsis = 0;
  for (int i = 0; i < sdim; ++i) {
    if (ellipsis_seen && ((1 << i) & new_axis_mask_) != 0x0)
      ++num_add_axis_after_ellipsis;
    if (((1 << i) & ellipsis_mask_) != 0x0)
      ellipsis_seen = true;
  }
  if (!ellipsis_seen) {
    ellipsis_mask_ |= (1 << sdim);
    ++sdim;
  }
  int ddim = input_dim_;
  *begin_mask = 0x0;
  *end_mask = 0x0;
  *shrink_axis_mask = 0x0;
  int fidx = 0;
  int iidx = 0;
  int tidx = 0;
  for (int i = 0; i < sdim; ++i) {
    if ((1 << i) & ellipsis_mask_) {
      int nidx =
          std::min(ddim - (sdim - i) + 1 + num_add_axis_after_ellipsis, ddim);
      for (; tidx < nidx; ++tidx, ++fidx) {
        begin_index[fidx] = 0;
        end_index[fidx] = 0;
        strides[fidx] = 1;
        input_shape[fidx] = input_shape_[iidx++];
        *begin_mask |= (1 << fidx);
        *end_mask |= (1 << fidx);
      }
    } else if ((1 << i) & new_axis_mask_) {
      begin_index[fidx] = 0;
      end_index[fidx] = 0;
      strides[fidx] = 1;
      input_shape[fidx] = 1;
      *begin_mask |= (1 << fidx);
      *end_mask |= (1 << fidx);
      ++fidx;
    } else {
      begin_index[fidx] = begin_index_[i];
      end_index[fidx] = end_index_[i];
      strides[fidx] = strides_[i];
      input_shape[fidx] = input_shape_[iidx++];
      if (begin_mask_ & (1 << i))
        *begin_mask |= (1 << fidx);
      if (end_mask_ & (1 << i))
        *end_mask |= (1 << fidx);
      if (shrink_axis_mask_ & (1 << i)) {
        *shrink_axis_mask |= (1 << fidx);
      }
      ++fidx;
      ++tidx;
    }
  }
  *input_dim = fidx;
}

inline int Clamp(int value, int min, int max) {
  value = value >= min ? value : min;
  value = value <= max ? value : max;
  return value;
}

int StartForAxis(const int *start_indices, const int *strides, const int mask,
                 const int *shape, const int axis) {
  const int axis_size = shape[axis];
  if (axis_size == 0) {
    return 0;
  }
  int start = start_indices[axis];

  if (mask & 1 << axis) {
    start = strides[axis] > 0 ? 0 : axis_size;
  }
  start = (start < 0) ? start + axis_size : start;

  if (strides[axis] > 0) {
    start = Clamp(start, 0, axis_size);
  } else {
    start = Clamp(start, -1, axis_size - 1);
  }
  return start;
}

int StopForAxis(const int *stop_indices, const int *strides, const int mask,
                const int shrink_mask, const int *shape, const int axis,
                int start_for_axis) {
  const int axis_size = shape[axis];
  if (axis_size == 0) {
    return 0;
  }
  const bool shrink_axis = shrink_mask & (1 << axis);
  int stop = stop_indices[axis];

  if (shrink_axis) {
    return start_for_axis + 1;
  }
  if (mask & (1 << axis)) {
    stop = strides[axis] < 0 ? 0 : axis_size;
  }
  stop = (stop < 0) ? stop + axis_size : stop;

  if (strides[axis] > 0) {
    stop = Clamp(stop, 0, axis_size);
  } else {
    stop = Clamp(stop, -1, axis_size - 1);
  }
  return stop;
}

void topk_indices(std::vector<std::pair<int, float>> &result,
                  const float *items, int num_elem, int k, bool largest) {
  using pair_t = std::pair<int, float>;
  auto cmp_large = [](pair_t const &item1, pair_t const &item2) {
    return (item1.second > item2.second)
          || (item1.second == item2.second && item1.first < item2.first);
  };
  auto cmp_small = [](pair_t const &item1, pair_t const &item2) {
    return (item1.second < item2.second)
          || (item1.second == item2.second && item1.first > item2.first);
  };
  auto cmp = largest ? cmp_large : cmp_small;
#if 0
  //heap_sort is a unstable sort
  std::priority_queue<pair_t, std::vector<pair_t>, decltype(cmp)> topk(cmp);
  for (int i = 0; i < num_elem; ++i) {
    if (topk.size() < k) {
      topk.emplace(i, items[i]);
    } else if (largest && items[i] <= topk.top().second) {
      continue;
    } else if (largest == false && items[i] >= topk.top().second) {
      continue;
    } else {
      topk.emplace(i, items[i]);
      topk.pop();
    }
  }
  while (!topk.empty()) {
    result.insert(result.begin(), pair_t(topk.top().first, topk.top().second));
    topk.pop();
  }
#endif
  std::vector<pair_t> topk;
  for (int i = 0; i < num_elem; i++)
    topk.emplace_back(i, items[i]);
  result.resize(k);
  std::partial_sort_copy(topk.begin(), topk.end(), result.begin(), result.end(), cmp);
}

std::vector<int64_t> shape_expand_dim(llvm::ArrayRef<int64_t> shape, int dims) {
  int diff = dims - shape.size();
  if (diff == 0)
    return shape.vec();
  std::vector<int64_t> shape_v(shape.begin(), shape.end());
  shape_v.insert(shape_v.begin(), diff, 1);
  return shape_v;
}

template <typename T> static int remove_value(std::vector<T> &v, int value) {
  int idx = 0;
  for (auto iter = v.begin(); iter != v.end(); iter++, idx++) {
    if (*iter == value) {
      v.erase(iter);
      return idx;
    }
  }
  return -1;
}

static void refresh(std::vector<int64_t> &order, int idx) {
  for (auto &v : order) {
    if (v > idx) {
      v--;
    }
  }
}

bool pad_reset(const std::vector<int64_t> &shape,
               const std::vector<int64_t> &pads, std::vector<int64_t> &shape_4,
               std::vector<int64_t> &pads_4) {
  pads_4.assign(pads.begin(), pads.end());
  shape_4.assign(shape.begin(), shape.end());
  if (shape_4.size() == 4) {
    return true;
  }
  int num_dims = shape_4.size();
  assert(pads_4.size() == 2 * num_dims);
  if (num_dims < 4) {
    int insert_dim = 4 - num_dims;
    for (int i = 0; i < insert_dim; i++) {
      shape_4.insert(shape_4.begin(), 1);
      pads_4.insert(pads_4.begin(), 0);
      pads_4.insert(pads_4.end() - num_dims, 0);
    }
    return true;
  }
  while (num_dims > 4) {
    bool done = false;
    for (int i = 0; i < num_dims - 1; i++) {
      if (pads_4[i] == 0 && pads_4[i + 1] == 0 && pads_4[i + num_dims] == 0 &&
          pads_4[i + num_dims + 1] == 0) {
        shape_4[i] *= shape_4[i + 1];
        shape_4.erase(shape_4.begin() + i + 1);
        pads_4.erase(pads_4.begin() + i + num_dims + 1);
        pads_4.erase(pads_4.begin() + i + 1);
        num_dims--;
        done = true;
        break;
      }
    }
    if (done == false) {
      break;
    }
  }
  if (num_dims != 4) {
    return false;
  }
  return true;
}

bool permute_reset(const std::vector<int64_t> &shape,
                   const std::vector<int64_t> &order,
                   std::vector<int64_t> &to_shape,
                   std::vector<int64_t> &to_order, int to_dim) {
  to_order.assign(order.begin(), order.end());
  to_shape.assign(shape.begin(), shape.end());
  int num_dims = shape.size();
  if (num_dims == to_dim) {
    return true;
  }
  if (num_dims > to_dim) {
    // remove dims = 1
    while (num_dims > to_dim) {
      int idx = remove_value(to_shape, 1);
      if (idx < 0) {
        break;
      }
      remove_value(to_order, idx);
      refresh(to_order, idx);
      num_dims--;
    }
    // remove continous order
    while (num_dims > to_dim) {
      bool done = false;
      for (int i = 0; i < num_dims - 1; i++) {
        if (to_order[i] + 1 == to_order[i + 1]) {
          int idx = to_order[i];
          to_shape[idx] *= to_shape[idx + 1];
          to_shape.erase(to_shape.begin() + idx + 1);
          to_order.erase(to_order.begin() + i + 1);
          refresh(to_order, idx + 1);
          num_dims--;
          done = true;
          break;
        }
      }
      if (done == false) {
        break;
      }
    }
    if (num_dims > to_dim) {
      return false;
    }
  } else if (num_dims < to_dim) {
    // reshape to  to_dims
    int inserted_dims = to_dim - num_dims;
    for (int i = 0; i < inserted_dims; i++) {
      to_shape.insert(to_shape.begin(), 1);
      to_order.insert(to_order.begin(), i);
    }

    for (int i = inserted_dims; i < to_dim; i++) {
      to_order[i] += inserted_dims;
    }
  }
  return true;
}

template <typename T>
void function_permute(T *from, T *to, const std::vector<int64_t> &shape_5,
                      const std::vector<int64_t> &order_5) {
  int64_t in = shape_5[0];
  int64_t ic = shape_5[1];
  int64_t id = shape_5[2];
  int64_t ih = shape_5[3];
  int64_t iw = shape_5[4];
  int64_t o0 = order_5[0];
  int64_t o1 = order_5[1];
  int64_t o2 = order_5[2];
  int64_t o3 = order_5[3];
  int64_t o4 = order_5[4];
  // clang-format on
  for (int n = 0; n < in; n++) {
    for (int c = 0; c < ic; c++) {
      for (int d = 0; d < id; d++) {
        for (int h = 0; h < ih; h++) {
          for (int w = 0; w < iw; w++) {
            int cur[5] = {n, c, d, h, w};
            int in_idx = w + h * iw + d * iw * ih + c * id * ih * iw +
                         n * ic * id * ih * iw;
            int out_idx =
                cur[o4] + cur[o3] * shape_5[o4] +
                cur[o2] * shape_5[o4] * shape_5[o3] +
                cur[o1] * shape_5[o4] * shape_5[o3] * shape_5[o2] +
                cur[o0] * shape_5[o4] * shape_5[o3] * shape_5[o2] * shape_5[o1];
            to[out_idx] = from[in_idx];
          }
        }
      }
    }
  }
  // clang-format off
}

template
void function_permute(float *from, float *to, const std::vector<int64_t> &shape_5,
                      const std::vector<int64_t> &order_5);

bool compare(float a, float b, llvm::StringRef mode) {
  if (mode == "Equal") {
    return a == b;
  }
  if (mode == "Greater") {
    return a > b;
  }
  if (mode == "GreaterOrEqual") {
    return a >= b;
  }
  if (mode == "Less") {
    return a < b;
  }
  if (mode == "LessOrEqual") {
    return a <= b;
  }
  llvm_unreachable("Not Implemented");
  return false;
}

// to compilable with gemmlowp
// fixedpoint/fixedpoint.h:exp_on_interval_between_negative_one_quarter_and_0_excl()
// return exp(x) for x in [-1/4, 0). Taylor expansion.
int32_t exp_on_interval_between_negative_one_quarter_and_0_excl(int input) {
  const int32_t const_term = 1895147668;
  const int32_t const_1_over_3 = 715827883;
  const int32_t const_1_over_8 = 1 << 28;
  int32_t x = input + const_1_over_8;

#define QUANT_MUL(x, y)                                                        \
  RightShiftRound((int64_t)x *(int64_t)y, 31, ROUNDING_HALF_UP);

  int32_t x2 = QUANT_MUL(x, x);
  int32_t x3 = QUANT_MUL(x2, x);
  int32_t x4 = QUANT_MUL(x2, x2);
  int32_t x4_over_4 = RightShiftRound(x4, 2, ROUNDING_HALF_AWAY_FROM_ZERO);
  int32_t x4_over_12_plus_x3_over_3_plus_x2 =
      QUANT_MUL((x4_over_4 + x3), const_1_over_3);
  int32_t x4_over_24_plus_x3_over_6_plus_x2_over_2 = RightShiftRound(
      x4_over_12_plus_x3_over_3_plus_x2, 1, ROUNDING_HALF_AWAY_FROM_ZERO);
  int32_t out =
      const_term +
      QUANT_MUL((x4_over_24_plus_x3_over_6_plus_x2_over_2 + x), const_term);
  return out;
}

// to compilable with gemmlowp
// fixedpoint/fixedpoint.h:exp_on_negative_values()
int32_t exp_on_negative_values(int input, int int_bits) {
  const int type_size = sizeof(int32_t); // input type is int32
  const int zero = 0;
  const int max_value = (1U << 31) - 1;
  const int total_bits = 8 * type_size;
  const int fract_bits = total_bits - 1 - int_bits;
  const int one = int_bits == 0 ? max_value : (1 << fract_bits);
  if (int_bits > 5) {
    int clamp_b = int_bits > 5 ? 36 - int_bits : 0;
    int clamp = clamp_b; // TODO: for input is 32bit; if input is 16bit,
                         // (clamp_b << 16), half away form zero
    if (input < clamp) {
      return zero;
    }
  }
  if (input == 0) {
    return one;
  }
  const int one_quarter = 1 << (fract_bits - 2);
  int mask = one_quarter - 1;
  int a_mod_quarter_minus_one_quarter = (input & mask) - one_quarter;
  int32_t result = exp_on_interval_between_negative_one_quarter_and_0_excl(
      a_mod_quarter_minus_one_quarter);
  int32_t remainder = a_mod_quarter_minus_one_quarter - input;

#define EXP_BARREL_SHIFTER(Exp, Multiplier)                                    \
  if (int_bits > Exp) {                                                        \
    int shift_num = fract_bits + Exp;                                          \
    if (remainder & (1 << shift_num)) {                                        \
      result = RightShiftRound((int64_t)result * (int64_t)Multiplier, 31,      \
                               ROUNDING_HALF_UP);                              \
    }                                                                          \
  }

  EXP_BARREL_SHIFTER(-2, 1672461947); // exp(-1/4)
  EXP_BARREL_SHIFTER(-1, 1302514674); // exp(-1/2)
  EXP_BARREL_SHIFTER(+0, 790015084);  // exp(-1)
  EXP_BARREL_SHIFTER(+1, 290630308);  // exp(-2)
  EXP_BARREL_SHIFTER(+2, 39332535);   // exp(-4)
  EXP_BARREL_SHIFTER(+3, 720401);     // exp(-8)
  EXP_BARREL_SHIFTER(+4, 242);        // exp(-16)

#undef EXP_BARREL_SHIFTER

  return result;
}

void swap_dim_data(float *input, float *output,
                   std::vector<int64_t> &ishape,
                   std::vector<int64_t> &offsets) {
  int axis = offsets.size();
  int64_t outer_size = 1, inner_size = 1;
  for (int i = 0; i < offsets.size(); ++i) {
    if (axis == offsets.size() && offsets[i] != 0) {
      axis = i;
    } else if (i < axis) {
      outer_size *= ishape[i];
    } else if (i > axis) {
      inner_size *= ishape[i];
    }
  }

  int first_part = offsets[axis] * inner_size;
  int second_part = (ishape[axis] - offsets[axis]) * inner_size;
  offsets[axis] = 0;
  for (int64_t i = 0; i < outer_size; ++i) {
    float *p_out = output + i * ishape[axis] * inner_size;
    float *p_in = input + i * ishape[axis] * inner_size;
    memcpy((void *)(p_out + second_part), (void *)p_in,
           first_part * sizeof(float));
    memcpy((void *)p_out, (void *)(p_in + first_part),
           second_part * sizeof(float));
  }
}

std::shared_ptr<std::vector<float>>
binary_add(float *a, float *b, const llvm::ArrayRef<int64_t> &a_shape,
           const llvm::ArrayRef<int64_t> &b_shape,
           std::vector<int64_t> &o_shape) {
  auto max_ndim = std::max(a_shape.size(), b_shape.size());
  auto a_shape_ = shape_expand_dim(a_shape, max_ndim);
  auto b_shape_ = shape_expand_dim(b_shape, max_ndim);
  o_shape.clear();
  for (int i = 0; i < max_ndim; i++) {
    o_shape.push_back(std::max(a_shape_[i], b_shape_[i]));
  }
  auto num_output = std::accumulate(o_shape.begin(), o_shape.end(), 1, std::multiplies<int64_t>());
  auto output = std::make_shared<std::vector<float>>(num_output);
  Binary add;
  add.lhs(a, a_shape_)
      .rhs(b, b_shape_)
      .dst(output->data(),o_shape)
      .algorithem(algorithm::binary_add)
      .setup();
  add.run();
  return std::move(output);
}

} // namespace tpu_mlir
