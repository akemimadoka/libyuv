/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "libyuv/row.h" /* For ARGBToAR30Row_AVX2 */

#include "libyuv/basic_types.h"
#include "libyuv/compare.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "../unit_test/unit_test.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/video_common.h"

#if defined(__arm__) || defined(__aarch64__)
// arm version subsamples by summing 4 pixels then multiplying by matrix with
// 4x smaller coefficients which are rounded to nearest integer.
#define ARM_YUV_ERROR 4
#else
#define ARM_YUV_ERROR 0
#endif

// Some functions fail on big endian. Enable these tests on all cpus except PowerPC
#if !defined(__powerpc__)
#define LITTLE_ENDIAN_TEST 1
#endif

namespace libyuv {

// Alias to copy pixels as is
#define AR30ToAR30 ARGBCopy
#define ABGRToABGR ARGBCopy

#define SUBSAMPLE(v, a) ((((v) + (a)-1)) / (a))

// Planar test

#define TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,         \
                       SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,             \
                       DST_SUBSAMP_X, DST_SUBSAMP_Y, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    static_assert(SRC_BPC == 1 || SRC_BPC == 2, "SRC BPC unsupported");       \
    static_assert(DST_BPC == 1 || DST_BPC == 2, "DST BPC unsupported");       \
    static_assert(SRC_SUBSAMP_X == 1 || SRC_SUBSAMP_X == 2,                   \
                  "DST SRC_SUBSAMP_X unsupported");                           \
    static_assert(SRC_SUBSAMP_Y == 1 || SRC_SUBSAMP_Y == 2,                   \
                  "DST SRC_SUBSAMP_Y unsupported");                           \
    static_assert(DST_SUBSAMP_X == 1 || DST_SUBSAMP_X == 2,                   \
                  "DST DST_SUBSAMP_X unsupported");                           \
    static_assert(DST_SUBSAMP_Y == 1 || DST_SUBSAMP_Y == 2,                   \
                  "DST DST_SUBSAMP_Y unsupported");                           \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    const int kSrcHalfWidth = SUBSAMPLE(kWidth, SRC_SUBSAMP_X);               \
    const int kSrcHalfHeight = SUBSAMPLE(kHeight, SRC_SUBSAMP_Y);             \
    const int kDstHalfWidth = SUBSAMPLE(kWidth, DST_SUBSAMP_X);               \
    const int kDstHalfHeight = SUBSAMPLE(kHeight, DST_SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight* SRC_BPC + OFF);             \
    align_buffer_page_end(src_u,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(src_v,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(dst_y_c, kWidth* kHeight* DST_BPC);                 \
    align_buffer_page_end(dst_u_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);   \
    align_buffer_page_end(dst_v_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);   \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight* DST_BPC);               \
    align_buffer_page_end(dst_u_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC); \
    align_buffer_page_end(dst_v_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC); \
    MemRandomize(src_y + OFF, kWidth * kHeight * SRC_BPC);                    \
    MemRandomize(src_u + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    MemRandomize(src_v + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    memset(dst_y_c, 1, kWidth* kHeight* DST_BPC);                             \
    memset(dst_u_c, 2, kDstHalfWidth* kDstHalfHeight* DST_BPC);               \
    memset(dst_v_c, 3, kDstHalfWidth* kDstHalfHeight* DST_BPC);               \
    memset(dst_y_opt, 101, kWidth* kHeight* DST_BPC);                         \
    memset(dst_u_opt, 102, kDstHalfWidth* kDstHalfHeight* DST_BPC);           \
    memset(dst_v_opt, 103, kDstHalfWidth* kDstHalfHeight* DST_BPC);           \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        reinterpret_cast<SRC_T*>(src_y + OFF), kWidth,                        \
        reinterpret_cast<SRC_T*>(src_u + OFF), kSrcHalfWidth,                 \
        reinterpret_cast<SRC_T*>(src_v + OFF), kSrcHalfWidth,                 \
        reinterpret_cast<DST_T*>(dst_y_c), kWidth,                            \
        reinterpret_cast<DST_T*>(dst_u_c), kDstHalfWidth,                     \
        reinterpret_cast<DST_T*>(dst_v_c), kDstHalfWidth, kWidth,             \
        NEG kHeight);                                                         \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          reinterpret_cast<SRC_T*>(src_y + OFF), kWidth,                      \
          reinterpret_cast<SRC_T*>(src_u + OFF), kSrcHalfWidth,               \
          reinterpret_cast<SRC_T*>(src_v + OFF), kSrcHalfWidth,               \
          reinterpret_cast<DST_T*>(dst_y_opt), kWidth,                        \
          reinterpret_cast<DST_T*>(dst_u_opt), kDstHalfWidth,                 \
          reinterpret_cast<DST_T*>(dst_v_opt), kDstHalfWidth, kWidth,         \
          NEG kHeight);                                                       \
    }                                                                         \
    for (int i = 0; i < kHeight * kWidth * DST_BPC; ++i) {                    \
      EXPECT_EQ(dst_y_c[i], dst_y_opt[i]);                                    \
    }                                                                         \
    for (int i = 0; i < kDstHalfWidth * kDstHalfHeight * DST_BPC; ++i) {      \
      EXPECT_EQ(dst_u_c[i], dst_u_opt[i]);                                    \
      EXPECT_EQ(dst_v_c[i], dst_v_opt[i]);                                    \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_u_c);                                    \
    free_aligned_buffer_page_end(dst_v_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_u_opt);                                  \
    free_aligned_buffer_page_end(dst_v_opt);                                  \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
  }

#define TESTPLANARTOP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,           \
                      SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,               \
                      DST_SUBSAMP_X, DST_SUBSAMP_Y)                            \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_ - 4, _Any, +, 0)                             \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Unaligned, +, 1)                           \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Invert, -, 0)                              \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Opt, +, 0)

TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I420, uint8_t, 1, 2, 2)
TESTPLANARTOP(I422, uint8_t, 1, 2, 1, I420, uint8_t, 1, 2, 2)
TESTPLANARTOP(I444, uint8_t, 1, 1, 1, I420, uint8_t, 1, 2, 2)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I422, uint8_t, 1, 2, 1)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I444, uint8_t, 1, 1, 1)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I420Mirror, uint8_t, 1, 2, 2)
TESTPLANARTOP(I422, uint8_t, 1, 2, 1, I422, uint8_t, 1, 2, 1)
TESTPLANARTOP(I444, uint8_t, 1, 1, 1, I444, uint8_t, 1, 1, 1)
TESTPLANARTOP(I010, uint16_t, 2, 2, 2, I010, uint16_t, 2, 2, 2)
TESTPLANARTOP(I010, uint16_t, 2, 2, 2, I420, uint8_t, 1, 2, 2)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I010, uint16_t, 2, 2, 2)
TESTPLANARTOP(H010, uint16_t, 2, 2, 2, H010, uint16_t, 2, 2, 2)
TESTPLANARTOP(H010, uint16_t, 2, 2, 2, H420, uint8_t, 1, 2, 2)
TESTPLANARTOP(H420, uint8_t, 1, 2, 2, H010, uint16_t, 2, 2, 2)

// Test Android 420 to I420
#define TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X,          \
                        SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                        W1280, N, NEG, OFF, PN, OFF_U, OFF_V)                 \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##_##PN##N) {       \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    const int kSizeUV =                                                       \
        SUBSAMPLE(kWidth, SRC_SUBSAMP_X) * SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_uv,                                             \
                          kSizeUV*((PIXEL_STRIDE == 3) ? 3 : 2) + OFF);       \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    uint8_t* src_u = src_uv + OFF_U;                                          \
    uint8_t* src_v = src_uv + (PIXEL_STRIDE == 1 ? kSizeUV : OFF_V);          \
    int src_stride_uv = SUBSAMPLE(kWidth, SUBSAMP_X) * PIXEL_STRIDE;          \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kWidth; ++j)                                        \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {             \
      for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {            \
        src_u[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
        src_v[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
      }                                                                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_u_c, 2,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_c, 3,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_u_opt, 102,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_opt, 103,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X),   \
        src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE, dst_y_c, \
        kWidth, dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X), dst_v_c,               \
        SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);                   \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), \
          src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE,        \
          dst_y_opt, kWidth, dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),         \
          dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);      \
    }                                                                         \
    int max_diff = 0;                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        int abs_diff = abs(static_cast<int>(dst_y_c[i * kWidth + j]) -        \
                           static_cast<int>(dst_y_opt[i * kWidth + j]));      \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_EQ(0, max_diff);                                                   \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        int abs_diff = abs(                                                   \
            static_cast<int>(dst_u_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]) - \
            static_cast<int>(                                                 \
                dst_u_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 3);                                                   \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        int abs_diff = abs(                                                   \
            static_cast<int>(dst_v_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]) - \
            static_cast<int>(                                                 \
                dst_v_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 3);                                                   \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_u_c);                                    \
    free_aligned_buffer_page_end(dst_v_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_u_opt);                                  \
    free_aligned_buffer_page_end(dst_v_opt);                                  \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_uv);                                     \
  }

#define TESTAPLANARTOP(SRC_FMT_PLANAR, PN, PIXEL_STRIDE, OFF_U, OFF_V,         \
                       SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X,    \
                       SUBSAMP_Y)                                              \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_ - 4,      \
                  _Any, +, 0, PN, OFF_U, OFF_V)                                \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_,          \
                  _Unaligned, +, 1, PN, OFF_U, OFF_V)                          \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, \
                  -, 0, PN, OFF_U, OFF_V)                                      \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, \
                  0, PN, OFF_U, OFF_V)

TESTAPLANARTOP(Android420, I420, 1, 0, 0, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV12, 2, 0, 1, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV21, 2, 1, 0, 2, 2, I420, 2, 2)

// wrapper to keep API the same
int I400ToNV21(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* /* src_u */,
               int /* src_stride_u */,
               const uint8_t* /* src_v */,
               int /* src_stride_v */,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return I400ToNV21(src_y, src_stride_y, dst_y, dst_stride_y, dst_vu,
                    dst_stride_vu, width, height);
}

#define TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,         \
                        FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG, OFF) \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_u, SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) +  \
                                     OFF);                                    \
    align_buffer_page_end(src_v, SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) +  \
                                     OFF);                                    \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_uv_c, SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *        \
                                        SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_uv_opt, SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *      \
                                          SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kWidth; ++j)                                        \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {             \
      for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {            \
        src_u[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =             \
            (fastrand() & 0xff);                                              \
        src_v[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =             \
            (fastrand() & 0xff);                                              \
      }                                                                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_uv_c, 2,                                                       \
           SUBSAMPLE(kWidth, SUBSAMP_X) * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y)); \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_uv_opt, 102,                                                   \
           SUBSAMPLE(kWidth, SUBSAMP_X) * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y)); \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X),   \
        src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), dst_y_c, kWidth,       \
        dst_uv_c, SUBSAMPLE(kWidth, SUBSAMP_X) * 2, kWidth, NEG kHeight);     \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), \
          src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), dst_y_opt, kWidth,   \
          dst_uv_opt, SUBSAMPLE(kWidth, SUBSAMP_X) * 2, kWidth, NEG kHeight); \
    }                                                                         \
    int max_diff = 0;                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        int abs_diff = abs(static_cast<int>(dst_y_c[i * kWidth + j]) -        \
                           static_cast<int>(dst_y_opt[i * kWidth + j]));      \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 1);                                                   \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X) * 2; ++j) {            \
        int abs_diff =                                                        \
            abs(static_cast<int>(                                             \
                    dst_uv_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) * 2 + j]) -     \
                static_cast<int>(                                             \
                    dst_uv_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) * 2 + j]));   \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 1);                                                   \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
  }

#define TESTPLANARTOBP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,        \
                       FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                    \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, \
                  SUBSAMP_X, SUBSAMP_Y, benchmark_width_ - 4, _Any, +, 0)   \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, \
                  SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Unaligned, +, 1) \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, \
                  SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, -, 0)    \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, \
                  SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, 0)

TESTPLANARTOBP(I420, 2, 2, NV12, 2, 2)
TESTPLANARTOBP(I420, 2, 2, NV21, 2, 2)
TESTPLANARTOBP(I422, 2, 1, NV21, 2, 2)
TESTPLANARTOBP(I444, 1, 1, NV21, 2, 2)
TESTPLANARTOBP(I400, 2, 2, NV21, 2, 2)

#define TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,       \
                          FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG,    \
                          OFF, DOY)                                           \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_uv, 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *      \
                                          SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + \
                                      OFF);                                   \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_uv_c, 2 * SUBSAMPLE(kWidth, SUBSAMP_X) *        \
                                        SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_uv_opt, 2 * SUBSAMPLE(kWidth, SUBSAMP_X) *      \
                                          SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kWidth; ++j)                                        \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {             \
      for (int j = 0; j < 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {        \
        src_uv[(i * 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =        \
            (fastrand() & 0xff);                                              \
      }                                                                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_uv_c, 2,                                                       \
           2 * SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y)); \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_uv_opt, 102,                                                   \
           2 * SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y)); \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y + OFF, kWidth, src_uv + OFF,                                    \
        2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X), DOY ? dst_y_c : NULL, kWidth,   \
        dst_uv_c, 2 * SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);     \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y + OFF, kWidth, src_uv + OFF,                                  \
          2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X), DOY ? dst_y_opt : NULL,       \
          kWidth, dst_uv_opt, 2 * SUBSAMPLE(kWidth, SUBSAMP_X), kWidth,       \
          NEG kHeight);                                                       \
    }                                                                         \
    int max_diff = 0;                                                         \
    if (DOY) {                                                                \
      for (int i = 0; i < kHeight; ++i) {                                     \
        for (int j = 0; j < kWidth; ++j) {                                    \
          int abs_diff = abs(static_cast<int>(dst_y_c[i * kWidth + j]) -      \
                             static_cast<int>(dst_y_opt[i * kWidth + j]));    \
          if (abs_diff > max_diff) {                                          \
            max_diff = abs_diff;                                              \
          }                                                                   \
        }                                                                     \
      }                                                                       \
      EXPECT_LE(max_diff, 1);                                                 \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < 2 * SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {            \
        int abs_diff =                                                        \
            abs(static_cast<int>(                                             \
                    dst_uv_c[i * 2 * SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -     \
                static_cast<int>(                                             \
                    dst_uv_opt[i * 2 * SUBSAMPLE(kWidth, SUBSAMP_X) + j]));   \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 1);                                                   \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_uv);                                     \
  }

#define TESTBIPLANARTOBP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,         \
                         FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                     \
  TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                    SUBSAMP_X, SUBSAMP_Y, benchmark_width_ - 4, _Any, +, 0, 1) \
  TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                    SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Unaligned, +, 1,  \
                    1)                                                         \
  TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                    SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, -, 0, 1)  \
  TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                    SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, 0, 1)     \
  TESTBIPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                    SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _NullY, +, 0, 0)

TESTBIPLANARTOBP(NV21, 2, 2, NV12, 2, 2)

#define TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,         \
                         FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG, OFF, \
                         DOY)                                                  \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {               \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = benchmark_height_;                                     \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_uv, 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *       \
                                          SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) +  \
                                      OFF);                                    \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X) *              \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));         \
    align_buffer_page_end(dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X) *              \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));         \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *            \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    align_buffer_page_end(dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *            \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kWidth; ++j)                                         \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                     \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {              \
      for (int j = 0; j < 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {         \
        src_uv[(i * 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =         \
            (fastrand() & 0xff);                                               \
      }                                                                        \
    }                                                                          \
    memset(dst_y_c, 1, kWidth* kHeight);                                       \
    memset(dst_u_c, 2,                                                         \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_v_c, 3,                                                         \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_y_opt, 101, kWidth* kHeight);                                   \
    memset(dst_u_opt, 102,                                                     \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_v_opt, 103,                                                     \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                            \
        src_y + OFF, kWidth, src_uv + OFF,                                     \
        2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X), DOY ? dst_y_c : NULL, kWidth,    \
        dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X), dst_v_c,                        \
        SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);                    \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                          \
          src_y + OFF, kWidth, src_uv + OFF,                                   \
          2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X), DOY ? dst_y_opt : NULL,        \
          kWidth, dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X), dst_v_opt,          \
          SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);                  \
    }                                                                          \
    int max_diff = 0;                                                          \
    if (DOY) {                                                                 \
      for (int i = 0; i < kHeight; ++i) {                                      \
        for (int j = 0; j < kWidth; ++j) {                                     \
          int abs_diff = abs(static_cast<int>(dst_y_c[i * kWidth + j]) -       \
                             static_cast<int>(dst_y_opt[i * kWidth + j]));     \
          if (abs_diff > max_diff) {                                           \
            max_diff = abs_diff;                                               \
          }                                                                    \
        }                                                                      \
      }                                                                        \
      EXPECT_LE(max_diff, 1);                                                  \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                 \
        int abs_diff = abs(                                                    \
            static_cast<int>(dst_u_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -  \
            static_cast<int>(                                                  \
                dst_u_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]));             \
        if (abs_diff > max_diff) {                                             \
          max_diff = abs_diff;                                                 \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, 1);                                                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                 \
        int abs_diff = abs(                                                    \
            static_cast<int>(dst_v_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -  \
            static_cast<int>(                                                  \
                dst_v_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]));             \
        if (abs_diff > max_diff) {                                             \
          max_diff = abs_diff;                                                 \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, 1);                                                    \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_u_c);                                     \
    free_aligned_buffer_page_end(dst_v_c);                                     \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_u_opt);                                   \
    free_aligned_buffer_page_end(dst_v_opt);                                   \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_uv);                                      \
  }

#define TESTBIPLANARTOP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,         \
                        FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                     \
  TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                   SUBSAMP_X, SUBSAMP_Y, benchmark_width_ - 4, _Any, +, 0, 1) \
  TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                   SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Unaligned, +, 1,  \
                   1)                                                         \
  TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                   SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, -, 0, 1)  \
  TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                   SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, 0, 1)     \
  TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR,  \
                   SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _NullY, +, 0, 0)

TESTBIPLANARTOP(NV12, 2, 2, I420, 2, 2)
TESTBIPLANARTOP(NV21, 2, 2, I420, 2, 2)

#define ALIGNINT(V, ALIGN) (((V) + (ALIGN)-1) / (ALIGN) * (ALIGN))

#define TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN, W1280, N, NEG, OFF)                            \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                       \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                  \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                     \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);            \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_u, kSizeUV + OFF);                              \
    align_buffer_page_end(src_v, kSizeUV + OFF);                              \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);               \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);             \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    for (int i = 0; i < kSizeUV; ++i) {                                       \
      src_u[i + OFF] = (fastrand() & 0xff);                                   \
      src_v[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                          \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                      \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    double time0 = get_time();                                                \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,        \
                          src_v + OFF, kStrideUV, dst_argb_c + OFF, kStrideB, \
                          kWidth, NEG kHeight);                               \
    double time1 = get_time();                                                \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,      \
                            src_v + OFF, kStrideUV, dst_argb_opt + OFF,       \
                            kStrideB, kWidth, NEG kHeight);                   \
    }                                                                         \
    double time2 = get_time();                                                \
    printf(" %8d us C - %8d us OPT\n",                                        \
           static_cast<int>((time1 - time0) * 1e6),                           \
           static_cast<int>((time2 - time1) * 1e6 / benchmark_iterations_));  \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                      \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_opt[i + OFF]);                  \
    }                                                                         \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#define TESTPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                      YALIGN)                                                \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_ - 4, _Any, +, 0)                   \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Unaligned, +, 1)                 \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Invert, -, 0)                    \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Opt, +, 0)

TESTPLANARTOB(I420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(J420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(J420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, BGRA, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RGBA, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(I420, 2, 2, RGB24, 3, 3, 1)
TESTPLANARTOB(H420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(H420, 2, 2, RGB24, 3, 3, 1)
#ifdef LITTLE_ENDIAN_TEST
TESTPLANARTOB(I420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(J420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(H420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB1555, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB4444, 2, 2, 1)
TESTPLANARTOB(I422, 2, 1, RGB565, 2, 2, 1)
#endif
TESTPLANARTOB(I422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(J422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(J422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(H422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(H422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, BGRA, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, RGBA, 4, 4, 1)
TESTPLANARTOB(I444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(J444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(I444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, YUY2, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, UYVY, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, YUY2, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, UYVY, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, I400, 1, 1, 1)
TESTPLANARTOB(J420, 2, 2, J400, 1, 1, 1)
#ifdef LITTLE_ENDIAN_TEST
TESTPLANARTOB(I420, 2, 2, AR30, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, AR30, 4, 4, 1)
#endif

#define TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                        YALIGN, W1280, DIFF, N, NEG, OFF, ATTEN)               \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(src_a, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
      src_a[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                       \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,         \
                          src_v + OFF, kStrideUV, src_a + OFF, kWidth,         \
                          dst_argb_c + OFF, kStrideB, kWidth, NEG kHeight,     \
                          ATTEN);                                              \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,       \
                            src_v + OFF, kStrideUV, src_a + OFF, kWidth,       \
                            dst_argb_opt + OFF, kStrideB, kWidth, NEG kHeight, \
                            ATTEN);                                            \
    }                                                                          \
    int max_diff = 0;                                                          \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                       \
      int abs_diff = abs(static_cast<int>(dst_argb_c[i + OFF]) -               \
                         static_cast<int>(dst_argb_opt[i + OFF]));             \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, DIFF);                                                 \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(src_a);                                       \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#define TESTQPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN, DIFF)                                          \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_ - 4, DIFF, _Any, +, 0, 0)          \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, DIFF, _Unaligned, +, 1, 0)        \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, DIFF, _Invert, -, 0, 0)           \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, DIFF, _Opt, +, 0, 0)              \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, DIFF, _Premult, +, 0, 1)

TESTQPLANARTOB(I420Alpha, 2, 2, ARGB, 4, 4, 1, 2)
TESTQPLANARTOB(I420Alpha, 2, 2, ABGR, 4, 4, 1, 2)

#define TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C,       \
                         BPP_B, W1280, DIFF, N, NEG, OFF)                      \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideB = kWidth * BPP_B;                                       \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_uv,                                              \
                          kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y) * 2 + OFF); \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight);                      \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight);                    \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kWidth; ++j)                                         \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                     \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < kStrideUV * 2; ++j) {                                \
        src_uv[i * kStrideUV * 2 + j + OFF] = (fastrand() & 0xff);             \
      }                                                                        \
    }                                                                          \
    memset(dst_argb_c, 1, kStrideB* kHeight);                                  \
    memset(dst_argb_opt, 101, kStrideB* kHeight);                              \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_uv + OFF, kStrideUV * 2,    \
                          dst_argb_c, kWidth * BPP_B, kWidth, NEG kHeight);    \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_uv + OFF, kStrideUV * 2,  \
                            dst_argb_opt, kWidth * BPP_B, kWidth,              \
                            NEG kHeight);                                      \
    }                                                                          \
    /* Convert to ARGB so 565 is expanded to bytes that can be compared. */    \
    align_buffer_page_end(dst_argb32_c, kWidth * 4 * kHeight);                 \
    align_buffer_page_end(dst_argb32_opt, kWidth * 4 * kHeight);               \
    memset(dst_argb32_c, 2, kWidth * 4 * kHeight);                             \
    memset(dst_argb32_opt, 102, kWidth * 4 * kHeight);                         \
    FMT_C##ToARGB(dst_argb_c, kStrideB, dst_argb32_c, kWidth * 4, kWidth,      \
                  kHeight);                                                    \
    FMT_C##ToARGB(dst_argb_opt, kStrideB, dst_argb32_opt, kWidth * 4, kWidth,  \
                  kHeight);                                                    \
    int max_diff = 0;                                                          \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth * 4; ++j) {                                   \
        int abs_diff =                                                         \
            abs(static_cast<int>(dst_argb32_c[i * kWidth * 4 + j]) -           \
                static_cast<int>(dst_argb32_opt[i * kWidth * 4 + j]));         \
        if (abs_diff > max_diff) {                                             \
          max_diff = abs_diff;                                                 \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, DIFF);                                                 \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_uv);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
    free_aligned_buffer_page_end(dst_argb32_c);                                \
    free_aligned_buffer_page_end(dst_argb32_opt);                              \
  }

#define TESTBIPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B, \
                        DIFF)                                                  \
  TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
                   benchmark_width_ - 4, DIFF, _Any, +, 0)                     \
  TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
                   benchmark_width_, DIFF, _Unaligned, +, 1)                   \
  TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
                   benchmark_width_, DIFF, _Invert, -, 0)                      \
  TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
                   benchmark_width_, DIFF, _Opt, +, 0)

TESTBIPLANARTOB(NV12, 2, 2, ARGB, ARGB, 4, 2)
TESTBIPLANARTOB(NV21, 2, 2, ARGB, ARGB, 4, 2)
TESTBIPLANARTOB(NV12, 2, 2, ABGR, ABGR, 4, 2)
TESTBIPLANARTOB(NV21, 2, 2, ABGR, ABGR, 4, 2)
TESTBIPLANARTOB(NV12, 2, 2, RGB24, RGB24, 3, 2)
TESTBIPLANARTOB(NV21, 2, 2, RGB24, RGB24, 3, 2)
TESTBIPLANARTOB(NV12, 2, 2, RAW, RAW, 3, 2)
TESTBIPLANARTOB(NV21, 2, 2, RAW, RAW, 3, 2)
#ifdef LITTLE_ENDIAN_TEST
TESTBIPLANARTOB(NV12, 2, 2, RGB565, RGB565, 2, 9)
#endif
TESTBIPLANARTOB(NV21, 2, 2, YUV24, RAW, 3, 2)

#ifdef DO_THREE_PLANES
// Do 3 allocations for yuv.  conventional but slower.
#define TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, \
                       W1280, DIFF, N, NEG, OFF)                               \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                        \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kStride = (kStrideUV * SUBSAMP_X * 8 * BPP_A + 7) / 8;           \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                   \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_u_c, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));  \
    align_buffer_page_end(dst_v_c, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));  \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_u_opt,                                           \
                          kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));           \
    align_buffer_page_end(dst_v_opt,                                           \
                          kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));           \
    memset(dst_y_c, 1, kWidth* kHeight);                                       \
    memset(dst_u_c, 2, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));              \
    memset(dst_v_c, 3, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));              \
    memset(dst_y_opt, 101, kWidth* kHeight);                                   \
    memset(dst_u_opt, 102, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));          \
    memset(dst_v_opt, 103, kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y));          \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kStride; ++j)                                        \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);               \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_u_c,   \
                          kStrideUV, dst_v_c, kStrideUV, kWidth, NEG kHeight); \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,        \
                            dst_u_opt, kStrideUV, dst_v_opt, kStrideUV,        \
                            kWidth, NEG kHeight);                              \
    }                                                                          \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth; ++j) {                                       \
        EXPECT_NEAR(static_cast<int>(dst_y_c[i * kWidth + j]),                 \
                    static_cast<int>(dst_y_opt[i * kWidth + j]), DIFF);        \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < kStrideUV; ++j) {                                    \
        EXPECT_NEAR(static_cast<int>(dst_u_c[i * kStrideUV + j]),              \
                    static_cast<int>(dst_u_opt[i * kStrideUV + j]), DIFF);     \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < kStrideUV; ++j) {                                    \
        EXPECT_NEAR(static_cast<int>(dst_v_c[i * kStrideUV + j]),              \
                    static_cast<int>(dst_v_opt[i * kStrideUV + j]), DIFF);     \
      }                                                                        \
    }                                                                          \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_u_c);                                     \
    free_aligned_buffer_page_end(dst_v_c);                                     \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_u_opt);                                   \
    free_aligned_buffer_page_end(dst_v_opt);                                   \
    free_aligned_buffer_page_end(src_argb);                                    \
  }
#else
#define TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, \
                       W1280, DIFF, N, NEG, OFF)                               \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                        \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kStride = (kStrideUV * SUBSAMP_X * 8 * BPP_A + 7) / 8;           \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                   \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_uv_c,                                            \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_uv_opt,                                          \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_y_c, 1, kWidth* kHeight);                                       \
    memset(dst_uv_c, 2, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    memset(dst_y_opt, 101, kWidth* kHeight);                                   \
    memset(dst_uv_opt, 102, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));    \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kStride; ++j)                                        \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);               \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_uv_c,  \
                          kStrideUV * 2, dst_uv_c + kStrideUV, kStrideUV * 2,  \
                          kWidth, NEG kHeight);                                \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,        \
                            dst_uv_opt, kStrideUV * 2, dst_uv_opt + kStrideUV, \
                            kStrideUV * 2, kWidth, NEG kHeight);               \
    }                                                                          \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth; ++j) {                                       \
        EXPECT_NEAR(static_cast<int>(dst_y_c[i * kWidth + j]),                 \
                    static_cast<int>(dst_y_opt[i * kWidth + j]), DIFF);        \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * 2; ++i) {              \
      for (int j = 0; j < kStrideUV; ++j) {                                    \
        EXPECT_NEAR(static_cast<int>(dst_uv_c[i * kStrideUV + j]),             \
                    static_cast<int>(dst_uv_opt[i * kStrideUV + j]), DIFF);    \
      }                                                                        \
    }                                                                          \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_uv_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_uv_opt);                                  \
    free_aligned_buffer_page_end(src_argb);                                    \
  }
#endif

#define TESTATOPLANAR(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, \
                      DIFF)                                                   \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_ - 4, DIFF, _Any, +, 0)                      \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, DIFF, _Unaligned, +, 1)                    \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, DIFF, _Invert, -, 0)                       \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, DIFF, _Opt, +, 0)

TESTATOPLANAR(ABGR, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(ARGB, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(ARGB, 4, 1, I422, 2, 1, 2)
TESTATOPLANAR(ARGB, 4, 1, I444, 1, 1, 2)
TESTATOPLANAR(ARGB, 4, 1, J420, 2, 2, ARM_YUV_ERROR)
TESTATOPLANAR(ARGB, 4, 1, J422, 2, 1, ARM_YUV_ERROR)
#ifdef LITTLE_ENDIAN_TEST
TESTATOPLANAR(ARGB1555, 2, 1, I420, 2, 2, 15)
TESTATOPLANAR(ARGB4444, 2, 1, I420, 2, 2, 17)
#endif
TESTATOPLANAR(BGRA, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(I400, 1, 1, I420, 2, 2, 2)
TESTATOPLANAR(J400, 1, 1, J420, 2, 2, 2)
TESTATOPLANAR(RAW, 3, 1, I420, 2, 2, 4)
TESTATOPLANAR(RGB24, 3, 1, I420, 2, 2, 4)
TESTATOPLANAR(RGB24, 3, 1, J420, 2, 2, ARM_YUV_ERROR)
#ifdef LITTLE_ENDIAN_TEST
TESTATOPLANAR(RGB565, 2, 1, I420, 2, 2, 5)
#endif
TESTATOPLANAR(RGBA, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(UYVY, 2, 1, I420, 2, 2, 2)
TESTATOPLANAR(UYVY, 2, 1, I422, 2, 1, 2)
TESTATOPLANAR(YUY2, 2, 1, I420, 2, 2, 2)
TESTATOPLANAR(YUY2, 2, 1, I422, 2, 1, 2)

#define TESTATOBIPLANARI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X,          \
                         SUBSAMP_Y, W1280, N, NEG, OFF)                       \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                       \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    const int kStride = SUBSAMPLE(kWidth, SUB_A) * BPP_A;                     \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                  \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_uv_c,                                           \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_uv_opt,                                         \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kStride; ++j)                                       \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);              \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_uv_c, 2, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_uv_opt, 102, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));   \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_uv_c, \
                          kStrideUV * 2, kWidth, NEG kHeight);                \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,       \
                            dst_uv_opt, kStrideUV * 2, kWidth, NEG kHeight);  \
    }                                                                         \
    int max_diff = 0;                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        int abs_diff = abs(static_cast<int>(dst_y_c[i * kWidth + j]) -        \
                           static_cast<int>(dst_y_opt[i * kWidth + j]));      \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 4);                                                   \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < kStrideUV * 2; ++j) {                               \
        int abs_diff =                                                        \
            abs(static_cast<int>(dst_uv_c[i * kStrideUV * 2 + j]) -           \
                static_cast<int>(dst_uv_opt[i * kStrideUV * 2 + j]));         \
        if (abs_diff > max_diff) {                                            \
          max_diff = abs_diff;                                                \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    EXPECT_LE(max_diff, 4);                                                   \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_argb);                                   \
  }

#define TESTATOBIPLANAR(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOBIPLANARI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                   benchmark_width_ - 4, _Any, +, 0)                           \
  TESTATOBIPLANARI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                   benchmark_width_, _Unaligned, +, 1)                         \
  TESTATOBIPLANARI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                   benchmark_width_, _Invert, -, 0)                            \
  TESTATOBIPLANARI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                   benchmark_width_, _Opt, +, 0)

TESTATOBIPLANAR(ARGB, 1, 4, NV12, 2, 2)
TESTATOBIPLANAR(ARGB, 1, 4, NV21, 2, 2)
TESTATOBIPLANAR(ABGR, 1, 4, NV12, 2, 2)
TESTATOBIPLANAR(ABGR, 1, 4, NV21, 2, 2)
TESTATOBIPLANAR(YUY2, 2, 4, NV12, 2, 2)
TESTATOBIPLANAR(UYVY, 2, 4, NV12, 2, 2)
TESTATOBIPLANAR(AYUV, 1, 4, NV12, 2, 2)
TESTATOBIPLANAR(AYUV, 1, 4, NV21, 2, 2)

#define TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,  \
                  HEIGHT_B, W1280, DIFF, N, NEG, OFF)                        \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##N) {                           \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                          \
    const int kHeight = benchmark_height_;                                   \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
    const int kStrideA =                                                     \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
    const int kStrideB =                                                     \
        (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
    align_buffer_page_end(src_argb, kStrideA* kHeightA + OFF);               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);                   \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);                 \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                          \
      src_argb[i + OFF] = (fastrand() & 0xff);                               \
    }                                                                        \
    memset(dst_argb_c, 1, kStrideB* kHeightB);                               \
    memset(dst_argb_opt, 101, kStrideB* kHeightB);                           \
    MaskCpuFlags(disable_cpu_flags_);                                        \
    FMT_A##To##FMT_B(src_argb + OFF, kStrideA, dst_argb_c, kStrideB, kWidth, \
                     NEG kHeight);                                           \
    MaskCpuFlags(benchmark_cpu_info_);                                       \
    for (int i = 0; i < benchmark_iterations_; ++i) {                        \
      FMT_A##To##FMT_B(src_argb + OFF, kStrideA, dst_argb_opt, kStrideB,     \
                       kWidth, NEG kHeight);                                 \
    }                                                                        \
    int max_diff = 0;                                                        \
    for (int i = 0; i < kStrideB * kHeightB; ++i) {                          \
      int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -                   \
                         static_cast<int>(dst_argb_opt[i]));                 \
      if (abs_diff > max_diff) {                                             \
        max_diff = abs_diff;                                                 \
      }                                                                      \
    }                                                                        \
    EXPECT_LE(max_diff, DIFF);                                               \
    free_aligned_buffer_page_end(src_argb);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                \
    free_aligned_buffer_page_end(dst_argb_opt);                              \
  }

#define TESTATOBRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B,     \
                       STRIDE_B, HEIGHT_B, DIFF)                           \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##_Random) {                   \
    for (int times = 0; times < benchmark_iterations_; ++times) {          \
      const int kWidth = (fastrand() & 63) + 1;                            \
      const int kHeight = (fastrand() & 31) + 1;                           \
      const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A; \
      const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B; \
      const int kStrideA =                                                 \
          (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;           \
      const int kStrideB =                                                 \
          (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;           \
      align_buffer_page_end(src_argb, kStrideA* kHeightA);                 \
      align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);               \
      align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);             \
      for (int i = 0; i < kStrideA * kHeightA; ++i) {                      \
        src_argb[i] = (fastrand() & 0xff);                                 \
      }                                                                    \
      memset(dst_argb_c, 123, kStrideB* kHeightB);                         \
      memset(dst_argb_opt, 123, kStrideB* kHeightB);                       \
      MaskCpuFlags(disable_cpu_flags_);                                    \
      FMT_A##To##FMT_B(src_argb, kStrideA, dst_argb_c, kStrideB, kWidth,   \
                       kHeight);                                           \
      MaskCpuFlags(benchmark_cpu_info_);                                   \
      FMT_A##To##FMT_B(src_argb, kStrideA, dst_argb_opt, kStrideB, kWidth, \
                       kHeight);                                           \
      for (int i = 0; i < kStrideB * kHeightB; ++i) {                      \
        EXPECT_NEAR(dst_argb_c[i], dst_argb_opt[i], DIFF);                 \
      }                                                                    \
      free_aligned_buffer_page_end(src_argb);                              \
      free_aligned_buffer_page_end(dst_argb_c);                            \
      free_aligned_buffer_page_end(dst_argb_opt);                          \
    }                                                                      \
  }

#define TESTATOB(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                 HEIGHT_B, DIFF)                                           \
  TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
            HEIGHT_B, benchmark_width_ - 4, DIFF, _Any, +, 0)              \
  TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
            HEIGHT_B, benchmark_width_, DIFF, _Unaligned, +, 1)            \
  TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
            HEIGHT_B, benchmark_width_, DIFF, _Invert, -, 0)               \
  TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
            HEIGHT_B, benchmark_width_, DIFF, _Opt, +, 0)                  \
  TESTATOBRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                 HEIGHT_B, DIFF)

// TODO(fbarchard): make ARM version of C code that matches NEON.
TESTATOB(AB30, 4, 4, 1, ABGR, 4, 4, 1, 0)
TESTATOB(AB30, 4, 4, 1, ARGB, 4, 4, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(ABGR, 4, 4, 1, AR30, 4, 4, 1, 0)
#endif
TESTATOB(ABGR, 4, 4, 1, ARGB, 4, 4, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(AR30, 4, 4, 1, AB30, 4, 4, 1, 0)
#endif
TESTATOB(AR30, 4, 4, 1, ABGR, 4, 4, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(AR30, 4, 4, 1, AR30, 4, 4, 1, 0)
TESTATOB(AR30, 4, 4, 1, ARGB, 4, 4, 1, 0)
#endif
TESTATOB(ARGB, 4, 4, 1, ABGR, 4, 4, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(ARGB, 4, 4, 1, AR30, 4, 4, 1, 0)
#endif
TESTATOB(ARGB, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGB1555, 2, 2, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGB4444, 2, 2, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGBMirror, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BGRA, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, I400, 1, 1, 1, 2)
TESTATOB(ARGB, 4, 4, 1, J400, 1, 1, 1, 2)
TESTATOB(ARGB, 4, 4, 1, RAW, 3, 3, 1, 0)
TESTATOB(ARGB, 4, 4, 1, RGB24, 3, 3, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(ARGB, 4, 4, 1, RGB565, 2, 2, 1, 0)
#endif
TESTATOB(ARGB, 4, 4, 1, RGBA, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, UYVY, 2, 4, 1, 4)
TESTATOB(ARGB, 4, 4, 1, YUY2, 2, 4, 1, 4)
TESTATOB(ARGB1555, 2, 2, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB4444, 2, 2, 1, ARGB, 4, 4, 1, 0)
TESTATOB(BGRA, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(I400, 1, 1, 1, ARGB, 4, 4, 1, 0)
TESTATOB(I400, 1, 1, 1, I400, 1, 1, 1, 0)
TESTATOB(I400, 1, 1, 1, I400Mirror, 1, 1, 1, 0)
TESTATOB(J400, 1, 1, 1, ARGB, 4, 4, 1, 0)
TESTATOB(J400, 1, 1, 1, J400, 1, 1, 1, 0)
TESTATOB(RAW, 3, 3, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RAW, 3, 3, 1, RGB24, 3, 3, 1, 0)
TESTATOB(RGB24, 3, 3, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RGB24, 3, 3, 1, J400, 1, 1, 1, 0)
#ifdef LITTLE_ENDIAN_TEST
TESTATOB(RGB565, 2, 2, 1, ARGB, 4, 4, 1, 0)
#endif
TESTATOB(RGBA, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(UYVY, 2, 4, 1, ARGB, 4, 4, 1, ARM_YUV_ERROR)
TESTATOB(YUY2, 2, 4, 1, ARGB, 4, 4, 1, ARM_YUV_ERROR)
TESTATOB(YUY2, 2, 4, 1, Y, 1, 1, 1, 0)

#define TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                   HEIGHT_B, W1280, DIFF, N, NEG, OFF)                       \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##Dither##N) {                   \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                          \
    const int kHeight = benchmark_height_;                                   \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
    const int kStrideA =                                                     \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
    const int kStrideB =                                                     \
        (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
    align_buffer_page_end(src_argb, kStrideA* kHeightA + OFF);               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);                   \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);                 \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                          \
      src_argb[i + OFF] = (fastrand() & 0xff);                               \
    }                                                                        \
    memset(dst_argb_c, 1, kStrideB* kHeightB);                               \
    memset(dst_argb_opt, 101, kStrideB* kHeightB);                           \
    MaskCpuFlags(disable_cpu_flags_);                                        \
    FMT_A##To##FMT_B##Dither(src_argb + OFF, kStrideA, dst_argb_c, kStrideB, \
                             NULL, kWidth, NEG kHeight);                     \
    MaskCpuFlags(benchmark_cpu_info_);                                       \
    for (int i = 0; i < benchmark_iterations_; ++i) {                        \
      FMT_A##To##FMT_B##Dither(src_argb + OFF, kStrideA, dst_argb_opt,       \
                               kStrideB, NULL, kWidth, NEG kHeight);         \
    }                                                                        \
    int max_diff = 0;                                                        \
    for (int i = 0; i < kStrideB * kHeightB; ++i) {                          \
      int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -                   \
                         static_cast<int>(dst_argb_opt[i]));                 \
      if (abs_diff > max_diff) {                                             \
        max_diff = abs_diff;                                                 \
      }                                                                      \
    }                                                                        \
    EXPECT_LE(max_diff, DIFF);                                               \
    free_aligned_buffer_page_end(src_argb);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                \
    free_aligned_buffer_page_end(dst_argb_opt);                              \
  }

#define TESTATOBDRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B,        \
                        STRIDE_B, HEIGHT_B, DIFF)                              \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##Dither_Random) {                 \
    for (int times = 0; times < benchmark_iterations_; ++times) {              \
      const int kWidth = (fastrand() & 63) + 1;                                \
      const int kHeight = (fastrand() & 31) + 1;                               \
      const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
      const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
      const int kStrideA =                                                     \
          (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
      const int kStrideB =                                                     \
          (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
      align_buffer_page_end(src_argb, kStrideA* kHeightA);                     \
      align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);                   \
      align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);                 \
      for (int i = 0; i < kStrideA * kHeightA; ++i) {                          \
        src_argb[i] = (fastrand() & 0xff);                                     \
      }                                                                        \
      memset(dst_argb_c, 123, kStrideB* kHeightB);                             \
      memset(dst_argb_opt, 123, kStrideB* kHeightB);                           \
      MaskCpuFlags(disable_cpu_flags_);                                        \
      FMT_A##To##FMT_B##Dither(src_argb, kStrideA, dst_argb_c, kStrideB, NULL, \
                               kWidth, kHeight);                               \
      MaskCpuFlags(benchmark_cpu_info_);                                       \
      FMT_A##To##FMT_B##Dither(src_argb, kStrideA, dst_argb_opt, kStrideB,     \
                               NULL, kWidth, kHeight);                         \
      int max_diff = 0;                                                        \
      for (int i = 0; i < kStrideB * kHeightB; ++i) {                          \
        int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -                   \
                           static_cast<int>(dst_argb_opt[i]));                 \
        if (abs_diff > max_diff) {                                             \
          max_diff = abs_diff;                                                 \
        }                                                                      \
      }                                                                        \
      EXPECT_LE(max_diff, DIFF);                                               \
      free_aligned_buffer_page_end(src_argb);                                  \
      free_aligned_buffer_page_end(dst_argb_c);                                \
      free_aligned_buffer_page_end(dst_argb_opt);                              \
    }                                                                          \
  }

#define TESTATOBD(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B, DIFF)                                           \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_ - 4, DIFF, _Any, +, 0)              \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, DIFF, _Unaligned, +, 1)            \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, DIFF, _Invert, -, 0)               \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, DIFF, _Opt, +, 0)                  \
  TESTATOBDRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B, DIFF)

#ifdef LITTLE_ENDIAN_TEST
TESTATOBD(ARGB, 4, 4, 1, RGB565, 2, 2, 1, 0)
#endif

#define TESTSYMI(FMT_ATOB, BPP_A, STRIDE_A, HEIGHT_A, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVConvertTest, FMT_ATOB##_Symetric##N) {                          \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = benchmark_height_;                                     \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;       \
    const int kStrideA =                                                       \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                 \
    align_buffer_page_end(src_argb, kStrideA* kHeightA + OFF);                 \
    align_buffer_page_end(dst_argb_c, kStrideA* kHeightA);                     \
    align_buffer_page_end(dst_argb_opt, kStrideA* kHeightA);                   \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                            \
      src_argb[i + OFF] = (fastrand() & 0xff);                                 \
    }                                                                          \
    memset(dst_argb_c, 1, kStrideA* kHeightA);                                 \
    memset(dst_argb_opt, 101, kStrideA* kHeightA);                             \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_ATOB(src_argb + OFF, kStrideA, dst_argb_c, kStrideA, kWidth,           \
             NEG kHeight);                                                     \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_ATOB(src_argb + OFF, kStrideA, dst_argb_opt, kStrideA, kWidth,       \
               NEG kHeight);                                                   \
    }                                                                          \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_ATOB(dst_argb_c, kStrideA, dst_argb_c, kStrideA, kWidth, NEG kHeight); \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    FMT_ATOB(dst_argb_opt, kStrideA, dst_argb_opt, kStrideA, kWidth,           \
             NEG kHeight);                                                     \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                            \
      EXPECT_EQ(src_argb[i + OFF], dst_argb_opt[i]);                           \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                               \
    }                                                                          \
    free_aligned_buffer_page_end(src_argb);                                    \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#define TESTSYM(FMT_ATOB, BPP_A, STRIDE_A, HEIGHT_A)                           \
  TESTSYMI(FMT_ATOB, BPP_A, STRIDE_A, HEIGHT_A, benchmark_width_ - 4, _Any, +, \
           0)                                                                  \
  TESTSYMI(FMT_ATOB, BPP_A, STRIDE_A, HEIGHT_A, benchmark_width_, _Unaligned,  \
           +, 1)                                                               \
  TESTSYMI(FMT_ATOB, BPP_A, STRIDE_A, HEIGHT_A, benchmark_width_, _Opt, +, 0)

TESTSYM(ARGBToARGB, 4, 4, 1)
TESTSYM(ARGBToBGRA, 4, 4, 1)
TESTSYM(ARGBToABGR, 4, 4, 1)
TESTSYM(BGRAToARGB, 4, 4, 1)
TESTSYM(ABGRToARGB, 4, 4, 1)

TEST_F(LibYUVConvertTest, Test565) {
  SIMD_ALIGNED(uint8_t orig_pixels[256][4]);
  SIMD_ALIGNED(uint8_t pixels565[256][2]);

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels[i][j] = i;
    }
  }
  ARGBToRGB565(&orig_pixels[0][0], 0, &pixels565[0][0], 0, 256, 1);
  uint32_t checksum = HashDjb2(&pixels565[0][0], sizeof(pixels565), 5381);
  EXPECT_EQ(610919429u, checksum);
}

#ifdef HAVE_JPEG
TEST_F(LibYUVConvertTest, ValidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_page_end(orig_pixels, kSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // Test special value that matches marker start.
  memset(orig_pixels, 0xff, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // EOI, SOI. Expect pass.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_TRUE(ValidateJpeg(orig_pixels, kSize));
  }
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, ValidateJpegLarge) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  const int kMultiple = 10;
  const int kBufSize = kImageSize * kMultiple + kOff;
  align_buffer_page_end(orig_pixels, kBufSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kBufSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kBufSize));

  // EOI, SOI. Expect pass.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_TRUE(ValidateJpeg(orig_pixels, kBufSize));
  }
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, InvalidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_page_end(orig_pixels, kSize);

  // NULL pointer. Expect fail.
  EXPECT_FALSE(ValidateJpeg(NULL, kSize));

  // Negative size. Expect fail.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, -1));

  // Too large size. Expect fail.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, 0xfb000000ull));

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // SOI but no EOI. Expect fail.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));
  }

  // EOI but no SOI. Expect fail.
  orig_pixels[0] = 0;
  orig_pixels[1] = 0;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, FuzzJpeg) {
  // SOI but no EOI. Expect fail.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    const int kSize = fastrand() % 5000 + 3;
    align_buffer_page_end(orig_pixels, kSize);
    MemRandomize(orig_pixels, kSize);

    // Add SOI so frame will be scanned.
    orig_pixels[0] = 0xff;
    orig_pixels[1] = 0xd8;  // SOI.
    orig_pixels[2] = 0xff;
    orig_pixels[kSize - 1] = 0xff;
    ValidateJpeg(orig_pixels,
                 kSize);  // Failure normally expected.
    free_aligned_buffer_page_end(orig_pixels);
  }
}

// Test data created in GIMP.  In export jpeg, disable
// thumbnails etc, choose a subsampling, and use low quality
// (50) to keep size small. Generated with xxd -i test.jpg
// test 0 is J400
static const uint8_t kTest0Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xc2, 0x00, 0x0b, 0x08, 0x00, 0x10,
    0x00, 0x20, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x17, 0x00, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x43, 0x7e, 0xa7, 0x97, 0x57, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0xc0, 0x6f, 0x66, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28,
    0x32, 0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4,
    0x00, 0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51,
    0x31, 0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x21, 0x65, 0x6e, 0x31, 0x86, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb,
    0xa9, 0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9,
    0xc6, 0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x10, 0x35, 0xff, 0xc4, 0x00, 0x1f, 0x10,
    0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91,
    0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x10, 0x0b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x88, 0xab, 0x8b,
    0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec,
    0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c,
    0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff,
    0xd9};
static const size_t kTest0JpgLen = 421;

// test 1 is J444
static const uint8_t kTest1Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x17, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xc4,
    0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x03, 0xff, 0xda,
    0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00, 0x01,
    0x40, 0x8f, 0x26, 0xe8, 0xf4, 0xcc, 0xf9, 0x69, 0x2b, 0x1b, 0x2a, 0xcb,
    0xff, 0xc4, 0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11,
    0x00, 0x03, 0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00,
    0x01, 0x05, 0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99,
    0x0d, 0x26, 0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x01, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x10, 0x11, 0x02, 0x12, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x03, 0x01, 0x01, 0x3f, 0x01, 0xf1, 0x00, 0x27, 0x45, 0xbb, 0x31,
    0xaf, 0xff, 0xc4, 0x00, 0x1a, 0x11, 0x00, 0x02, 0x03, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x02, 0x10, 0x11, 0x41, 0x12, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01,
    0x01, 0x3f, 0x01, 0xf6, 0x4b, 0x5f, 0x48, 0xb3, 0x69, 0x63, 0x35, 0x72,
    0xbf, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda, 0x00,
    0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32, 0xd2,
    0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00, 0x1c,
    0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31, 0x61,
    0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x21,
    0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9, 0x01,
    0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6, 0x48,
    0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
    0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x26, 0x61, 0xd4, 0xff,
    0xc4, 0x00, 0x1a, 0x11, 0x00, 0x03, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x21,
    0x31, 0x41, 0x51, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f,
    0x10, 0x54, 0xa8, 0xbf, 0x50, 0x87, 0xb0, 0x9d, 0x8b, 0xc4, 0x6a, 0x26,
    0x6b, 0x2a, 0x9c, 0x1f, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x11, 0x21, 0x51, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02,
    0x01, 0x01, 0x3f, 0x10, 0x70, 0xe1, 0x3e, 0xd1, 0x8e, 0x0d, 0xe1, 0xb5,
    0xd5, 0x91, 0x76, 0x43, 0x82, 0x45, 0x4c, 0x7b, 0x7f, 0xff, 0xc4, 0x00,
    0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61,
    0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01,
    0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x8a,
    0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96,
    0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad,
    0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7,
    0xd4, 0xff, 0xd9};
static const size_t kTest1JpgLen = 735;

// test 2 is J420
static const uint8_t kTest2Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x18, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x05, 0x01, 0x02, 0x04, 0xff,
    0xc4, 0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0x20, 0xe7, 0x28, 0xa3, 0x0b, 0x2e, 0x2d, 0xcf, 0xff, 0xc4, 0x00,
    0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03, 0x10,
    0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05, 0x02,
    0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26, 0x62,
    0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f,
    0x01, 0xc8, 0x53, 0xff, 0xc4, 0x00, 0x16, 0x11, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x32, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3f,
    0x01, 0xd2, 0xc7, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28,
    0x32, 0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4,
    0x00, 0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51,
    0x31, 0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x21, 0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb,
    0xa9, 0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9,
    0xc6, 0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c,
    0x03, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x13, 0x5f,
    0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11,
    0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x0e,
    0xa1, 0x3a, 0x76, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x21, 0x11, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01,
    0x3f, 0x10, 0x57, 0x0b, 0x08, 0x70, 0xdb, 0xff, 0xc4, 0x00, 0x1f, 0x10,
    0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91,
    0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x8a, 0xeb, 0x8b,
    0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec,
    0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c,
    0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff,
    0xd9};
static const size_t kTest2JpgLen = 685;

// test 3 is J422
static const uint8_t kTest3Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x17, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xc4,
    0x00, 0x17, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x00, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0x43, 0x8d, 0x1f, 0xa2, 0xb3, 0xca, 0x1b, 0x57, 0x0f, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x00, 0x02, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x10, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03,
    0x01, 0x01, 0x3f, 0x01, 0x51, 0xce, 0x8c, 0x75, 0xff, 0xc4, 0x00, 0x18,
    0x11, 0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x61, 0x21, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3f, 0x01, 0xa6, 0xd9, 0x2f, 0x84,
    0xe8, 0xf0, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32,
    0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00,
    0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31,
    0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f,
    0x21, 0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9,
    0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6,
    0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03,
    0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x2e, 0x45, 0xff,
    0xc4, 0x00, 0x18, 0x11, 0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x21,
    0x31, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x53,
    0x50, 0xba, 0x54, 0xc1, 0x67, 0x4f, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x11, 0x21, 0x00, 0x10, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x02, 0x01, 0x01, 0x3f, 0x10, 0x18, 0x81, 0x5c, 0x04, 0x1a, 0xca,
    0x91, 0xbf, 0xff, 0xc4, 0x00, 0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9,
    0x58, 0xbe, 0x1a, 0xfd, 0x8a, 0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5,
    0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c,
    0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00,
    0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff, 0xd9};
static const size_t kTest3JpgLen = 704;

// test 4 is J422 vertical - not supported
static const uint8_t kTest4Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x12, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x18, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x02, 0x03, 0xff,
    0xc4, 0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0xd2, 0x98, 0xe9, 0x03, 0x0c, 0x00, 0x46, 0x21, 0xd9, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x01, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01,
    0x3f, 0x01, 0x98, 0xb1, 0xbd, 0x47, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x12, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x02, 0x01, 0x01, 0x3f, 0x01, 0xb6, 0x35, 0xa2, 0xe1, 0x47, 0xff,
    0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x21, 0x02,
    0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32, 0xd2, 0xed, 0xf9,
    0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00, 0x1c, 0x10, 0x01,
    0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31, 0x61, 0x81, 0xf0,
    0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x21, 0x75, 0x6e,
    0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9, 0x01, 0xf3, 0xde,
    0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6, 0x48, 0x5d, 0x7a,
    0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x24, 0xaf, 0xff, 0xc4, 0x00, 0x19,
    0x11, 0x00, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x51, 0x21, 0x31, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x59, 0x11, 0xca,
    0x42, 0x60, 0x9f, 0x69, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x00, 0x02, 0x03,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x11, 0x21, 0x31, 0x61, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x02, 0x01, 0x01, 0x3f, 0x10, 0xb0, 0xd7, 0x27, 0x51, 0xb6, 0x41, 0xff,
    0xc4, 0x00, 0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31,
    0x41, 0x61, 0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a,
    0xfd, 0x8a, 0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd,
    0x46, 0x96, 0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30,
    0x49, 0xad, 0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03,
    0x0b, 0xb7, 0xd4, 0xff, 0xd9};
static const size_t kTest4JpgLen = 701;

TEST_F(LibYUVConvertTest, TestMJPGSize) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  printf("test jpeg size %d x %d\n", width, height);
}

TEST_F(LibYUVConvertTest, TestMJPGToI420) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_u, half_width * half_height);
  align_buffer_page_end(dst_v, half_width * half_height);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToI420(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_u, half_width,
                     dst_v, half_width, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_u_hash = HashDjb2(dst_u, half_width * half_height, 5381);
  uint32_t dst_v_hash = HashDjb2(dst_v, half_width * half_height, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_u_hash, 2501859930u);
  EXPECT_EQ(dst_v_hash, 2126459123u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
}

TEST_F(LibYUVConvertTest, TestMJPGToI420_NV21) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  // Convert to NV21
  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);

  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_vu,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert to I420
  align_buffer_page_end(dst2_y, width * height);
  align_buffer_page_end(dst2_u, half_width * half_height);
  align_buffer_page_end(dst2_v, half_width * half_height);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToI420(kTest2Jpg, kTest2JpgLen, dst2_y, width, dst2_u, half_width,
                     dst2_v, half_width, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert I420 to NV21
  align_buffer_page_end(dst3_y, width * height);
  align_buffer_page_end(dst3_vu, half_width * half_height * 2);

  I420ToNV21(dst2_y, width, dst2_u, half_width, dst2_v, half_width, dst3_y,
             width, dst3_vu, half_width * 2, width, height);

  for (int i = 0; i < width * height; ++i) {
    EXPECT_EQ(dst_y[i], dst3_y[i]);
  }
  for (int i = 0; i < half_width * half_height * 2; ++i) {
    EXPECT_EQ(dst_vu[i], dst3_vu[i]);
    EXPECT_EQ(dst_vu[i], dst3_vu[i]);
  }

  free_aligned_buffer_page_end(dst3_y);
  free_aligned_buffer_page_end(dst3_vu);

  free_aligned_buffer_page_end(dst2_y);
  free_aligned_buffer_page_end(dst2_u);
  free_aligned_buffer_page_end(dst2_v);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_vu);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_420) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 1069662856u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_422) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest3Jpg, kTest3JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest3Jpg, kTest3JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 3543430771u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_400) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest0Jpg, kTest0JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest0Jpg, kTest0JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 330644005u);
  EXPECT_EQ(dst_uv_hash, 135214341u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_444) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest1Jpg, kTest1JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest1Jpg, kTest1JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 506143297u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToARGB) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest3Jpg, kTest3JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);

  align_buffer_page_end(dst_argb, width * height * 4);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToARGB(kTest3Jpg, kTest3JpgLen, dst_argb, width * 4, width,
                     height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_argb_hash = HashDjb2(dst_argb, width * height, 5381);
  EXPECT_EQ(dst_argb_hash, 2355976473u);

  free_aligned_buffer_page_end(dst_argb);
}

static int ShowJPegInfo(const uint8_t* sample, size_t sample_size) {
  MJpegDecoder mjpeg_decoder;
  LIBYUV_BOOL ret = mjpeg_decoder.LoadFrame(sample, sample_size);

  int width = mjpeg_decoder.GetWidth();
  int height = mjpeg_decoder.GetHeight();

  // YUV420
  if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
      mjpeg_decoder.GetNumComponents() == 3 &&
      mjpeg_decoder.GetVertSampFactor(0) == 2 &&
      mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
      mjpeg_decoder.GetVertSampFactor(1) == 1 &&
      mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
      mjpeg_decoder.GetVertSampFactor(2) == 1 &&
      mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J420, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV422
  } else if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
             mjpeg_decoder.GetNumComponents() == 3 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
             mjpeg_decoder.GetVertSampFactor(1) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
             mjpeg_decoder.GetVertSampFactor(2) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J422, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV444
  } else if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
             mjpeg_decoder.GetNumComponents() == 3 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 1 &&
             mjpeg_decoder.GetVertSampFactor(1) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
             mjpeg_decoder.GetVertSampFactor(2) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J444, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV400
  } else if (mjpeg_decoder.GetColorSpace() ==
                 MJpegDecoder::kColorSpaceGrayscale &&
             mjpeg_decoder.GetNumComponents() == 1 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 1) {
    printf("JPeg is J400, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
  } else {
    // Unknown colorspace.
    printf("JPeg is Unknown colorspace.\n");
  }
  mjpeg_decoder.UnloadFrame();
  return ret;
}

TEST_F(LibYUVConvertTest, TestMJPGInfo) {
  EXPECT_EQ(1, ShowJPegInfo(kTest0Jpg, kTest0JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest1Jpg, kTest1JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest2Jpg, kTest2JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest3Jpg, kTest3JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest4Jpg,
                            kTest4JpgLen));  // Valid but unsupported.
}
#endif  // HAVE_JPEG

TEST_F(LibYUVConvertTest, NV12Crop) {
  const int SUBSAMP_X = 2;
  const int SUBSAMP_Y = 2;
  const int kWidth = benchmark_width_;
  const int kHeight = benchmark_height_;
  const int crop_y =
      ((benchmark_height_ - (benchmark_height_ * 360 / 480)) / 2 + 1) & ~1;
  const int kDestWidth = benchmark_width_;
  const int kDestHeight = benchmark_height_ - crop_y * 2;
  const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int sample_size =
      kWidth * kHeight + kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y) * 2;
  align_buffer_page_end(src_y, sample_size);
  uint8_t* src_uv = src_y + kWidth * kHeight;

  align_buffer_page_end(dst_y, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  align_buffer_page_end(dst_y_2, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u_2, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                     SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v_2, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                     SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  for (int i = 0; i < kHeight * kWidth; ++i) {
    src_y[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < (SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideUV) * 2; ++i) {
    src_uv[i] = (fastrand() & 0xff);
  }
  memset(dst_y, 1, kDestWidth * kDestHeight);
  memset(dst_u, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_y_2, 1, kDestWidth * kDestHeight);
  memset(dst_u_2, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v_2, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  ConvertToI420(src_y, sample_size, dst_y_2, kDestWidth, dst_u_2,
                SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v_2,
                SUBSAMPLE(kDestWidth, SUBSAMP_X), 0, crop_y, kWidth, kHeight,
                kDestWidth, kDestHeight, libyuv::kRotate0, libyuv::FOURCC_NV12);

  NV12ToI420(src_y + crop_y * kWidth, kWidth,
             src_uv + (crop_y / 2) * kStrideUV * 2, kStrideUV * 2, dst_y,
             kDestWidth, dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v,
             SUBSAMPLE(kDestWidth, SUBSAMP_X), kDestWidth, kDestHeight);

  for (int i = 0; i < kDestHeight; ++i) {
    for (int j = 0; j < kDestWidth; ++j) {
      EXPECT_EQ(dst_y[i * kWidth + j], dst_y_2[i * kWidth + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(dst_u[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j],
                dst_u_2[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(dst_v[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j],
                dst_v_2[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(dst_y_2);
  free_aligned_buffer_page_end(dst_u_2);
  free_aligned_buffer_page_end(dst_v_2);
  free_aligned_buffer_page_end(src_y);
}

TEST_F(LibYUVConvertTest, I420CropOddY) {
  const int SUBSAMP_X = 2;
  const int SUBSAMP_Y = 2;
  const int kWidth = benchmark_width_;
  const int kHeight = benchmark_height_;
  const int crop_y = 1;
  const int kDestWidth = benchmark_width_;
  const int kDestHeight = benchmark_height_ - crop_y * 2;
  const int kStrideU = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int kStrideV = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int sample_size = kWidth * kHeight +
                          kStrideU * SUBSAMPLE(kHeight, SUBSAMP_Y) +
                          kStrideV * SUBSAMPLE(kHeight, SUBSAMP_Y);
  align_buffer_page_end(src_y, sample_size);
  uint8_t* src_u = src_y + kWidth * kHeight;
  uint8_t* src_v = src_u + kStrideU * SUBSAMPLE(kHeight, SUBSAMP_Y);

  align_buffer_page_end(dst_y, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  for (int i = 0; i < kHeight * kWidth; ++i) {
    src_y[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideU; ++i) {
    src_u[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideV; ++i) {
    src_v[i] = (fastrand() & 0xff);
  }
  memset(dst_y, 1, kDestWidth * kDestHeight);
  memset(dst_u, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    ConvertToI420(src_y, sample_size, dst_y, kDestWidth, dst_u,
                  SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v,
                  SUBSAMPLE(kDestWidth, SUBSAMP_X), 0, crop_y, kWidth, kHeight,
                  kDestWidth, kDestHeight, libyuv::kRotate0,
                  libyuv::FOURCC_I420);
  }

  for (int i = 0; i < kDestHeight; ++i) {
    for (int j = 0; j < kDestWidth; ++j) {
      EXPECT_EQ(src_y[crop_y * kWidth + i * kWidth + j],
                dst_y[i * kDestWidth + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(src_u[(crop_y / 2 + i) * kStrideU + j],
                dst_u[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(src_v[(crop_y / 2 + i) * kStrideV + j],
                dst_v[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(src_y);
}

TEST_F(LibYUVConvertTest, TestYToARGB) {
  uint8_t y[32];
  uint8_t expectedg[32];
  for (int i = 0; i < 32; ++i) {
    y[i] = i * 5 + 17;
    expectedg[i] = static_cast<int>((y[i] - 16) * 1.164f + 0.5f);
  }
  uint8_t argb[32 * 4];
  YToARGB(y, 0, argb, 0, 32, 1);

  for (int i = 0; i < 32; ++i) {
    printf("%2d %d: %d <-> %d,%d,%d,%d\n", i, y[i], expectedg[i],
           argb[i * 4 + 0], argb[i * 4 + 1], argb[i * 4 + 2], argb[i * 4 + 3]);
  }
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(expectedg[i], argb[i * 4 + 0]);
  }
}

static const uint8_t kNoDither4x4[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

TEST_F(LibYUVConvertTest, TestNoDither) {
  align_buffer_page_end(src_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_rgb565dither,
                        benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(src_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_rgb565dither, benchmark_width_ * benchmark_height_ * 2);
  ARGBToRGB565(src_argb, benchmark_width_ * 4, dst_rgb565, benchmark_width_ * 2,
               benchmark_width_, benchmark_height_);
  ARGBToRGB565Dither(src_argb, benchmark_width_ * 4, dst_rgb565dither,
                     benchmark_width_ * 2, kNoDither4x4, benchmark_width_,
                     benchmark_height_);
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 2; ++i) {
    EXPECT_EQ(dst_rgb565[i], dst_rgb565dither[i]);
  }

  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_rgb565);
  free_aligned_buffer_page_end(dst_rgb565dither);
}

// Ordered 4x4 dither for 888 to 565.  Values from 0 to 7.
static const uint8_t kDither565_4x4[16] = {
    0, 4, 1, 5, 6, 2, 7, 3, 1, 5, 0, 4, 7, 3, 6, 2,
};

TEST_F(LibYUVConvertTest, TestDither) {
  align_buffer_page_end(src_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_rgb565dither,
                        benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_argbdither,
                        benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(src_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_rgb565dither, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_argbdither, benchmark_width_ * benchmark_height_ * 4);
  ARGBToRGB565(src_argb, benchmark_width_ * 4, dst_rgb565, benchmark_width_ * 2,
               benchmark_width_, benchmark_height_);
  ARGBToRGB565Dither(src_argb, benchmark_width_ * 4, dst_rgb565dither,
                     benchmark_width_ * 2, kDither565_4x4, benchmark_width_,
                     benchmark_height_);
  RGB565ToARGB(dst_rgb565, benchmark_width_ * 2, dst_argb, benchmark_width_ * 4,
               benchmark_width_, benchmark_height_);
  RGB565ToARGB(dst_rgb565dither, benchmark_width_ * 2, dst_argbdither,
               benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 4; ++i) {
    EXPECT_NEAR(dst_argb[i], dst_argbdither[i], 9);
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_rgb565);
  free_aligned_buffer_page_end(dst_rgb565dither);
  free_aligned_buffer_page_end(dst_argb);
  free_aligned_buffer_page_end(dst_argbdither);
}

#define TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                        YALIGN, W1280, DIFF, N, NEG, OFF, FMT_C, BPP_C)        \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##Dither##N) {                \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                       \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B##Dither(src_y + OFF, kWidth, src_u + OFF, kStrideUV, \
                                  src_v + OFF, kStrideUV, dst_argb_c + OFF,    \
                                  kStrideB, NULL, kWidth, NEG kHeight);        \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B##Dither(                                           \
          src_y + OFF, kWidth, src_u + OFF, kStrideUV, src_v + OFF, kStrideUV, \
          dst_argb_opt + OFF, kStrideB, NULL, kWidth, NEG kHeight);            \
    }                                                                          \
    int max_diff = 0;                                                          \
    /* Convert to ARGB so 565 is expanded to bytes that can be compared. */    \
    align_buffer_page_end(dst_argb32_c, kWidth* BPP_C* kHeight);               \
    align_buffer_page_end(dst_argb32_opt, kWidth* BPP_C* kHeight);             \
    memset(dst_argb32_c, 2, kWidth* BPP_C* kHeight);                           \
    memset(dst_argb32_opt, 102, kWidth* BPP_C* kHeight);                       \
    FMT_B##To##FMT_C(dst_argb_c + OFF, kStrideB, dst_argb32_c, kWidth * BPP_C, \
                     kWidth, kHeight);                                         \
    FMT_B##To##FMT_C(dst_argb_opt + OFF, kStrideB, dst_argb32_opt,             \
                     kWidth * BPP_C, kWidth, kHeight);                         \
    for (int i = 0; i < kWidth * BPP_C * kHeight; ++i) {                       \
      int abs_diff = abs(static_cast<int>(dst_argb32_c[i]) -                   \
                         static_cast<int>(dst_argb32_opt[i]));                 \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, DIFF);                                                 \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
    free_aligned_buffer_page_end(dst_argb32_c);                                \
    free_aligned_buffer_page_end(dst_argb32_opt);                              \
  }

#define TESTPLANARTOBD(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                       YALIGN, DIFF, FMT_C, BPP_C)                             \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,       \
                  YALIGN, benchmark_width_ - 4, DIFF, _Any, +, 0, FMT_C,       \
                  BPP_C)                                                       \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,       \
                  YALIGN, benchmark_width_, DIFF, _Unaligned, +, 1, FMT_C,     \
                  BPP_C)                                                       \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,       \
                  YALIGN, benchmark_width_, DIFF, _Invert, -, 0, FMT_C, BPP_C) \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,       \
                  YALIGN, benchmark_width_, DIFF, _Opt, +, 0, FMT_C, BPP_C)

#ifdef LITTLE_ENDIAN_TEST
TESTPLANARTOBD(I420, 2, 2, RGB565, 2, 2, 1, 9, ARGB, 4)
#endif
#define TESTPTOB(NAME, UYVYTOI420, UYVYTONV12)                                \
  TEST_F(LibYUVConvertTest, NAME) {                                           \
    const int kWidth = benchmark_width_;                                      \
    const int kHeight = benchmark_height_;                                    \
                                                                              \
    align_buffer_page_end(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2) * kHeight);     \
    align_buffer_page_end(orig_y, kWidth* kHeight);                           \
    align_buffer_page_end(orig_u,                                             \
                          SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));      \
    align_buffer_page_end(orig_v,                                             \
                          SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));      \
                                                                              \
    align_buffer_page_end(dst_y_orig, kWidth* kHeight);                       \
    align_buffer_page_end(dst_uv_orig,                                        \
                          2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));  \
                                                                              \
    align_buffer_page_end(dst_y, kWidth* kHeight);                            \
    align_buffer_page_end(dst_uv,                                             \
                          2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));  \
                                                                              \
    MemRandomize(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2) * kHeight);              \
                                                                              \
    /* Convert UYVY to NV12 in 2 steps for reference */                       \
    libyuv::UYVYTOI420(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2), orig_y, kWidth,   \
                       orig_u, SUBSAMPLE(kWidth, 2), orig_v,                  \
                       SUBSAMPLE(kWidth, 2), kWidth, kHeight);                \
    libyuv::I420ToNV12(orig_y, kWidth, orig_u, SUBSAMPLE(kWidth, 2), orig_v,  \
                       SUBSAMPLE(kWidth, 2), dst_y_orig, kWidth, dst_uv_orig, \
                       2 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);            \
                                                                              \
    /* Convert to NV12 */                                                     \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      libyuv::UYVYTONV12(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2), dst_y, kWidth,  \
                         dst_uv, 2 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);  \
    }                                                                         \
                                                                              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      EXPECT_EQ(orig_y[i], dst_y[i]);                                         \
    }                                                                         \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      EXPECT_EQ(dst_y_orig[i], dst_y[i]);                                     \
    }                                                                         \
    for (int i = 0; i < 2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2);     \
         ++i) {                                                               \
      EXPECT_EQ(dst_uv_orig[i], dst_uv[i]);                                   \
    }                                                                         \
                                                                              \
    free_aligned_buffer_page_end(orig_uyvy);                                  \
    free_aligned_buffer_page_end(orig_y);                                     \
    free_aligned_buffer_page_end(orig_u);                                     \
    free_aligned_buffer_page_end(orig_v);                                     \
    free_aligned_buffer_page_end(dst_y_orig);                                 \
    free_aligned_buffer_page_end(dst_uv_orig);                                \
    free_aligned_buffer_page_end(dst_y);                                      \
    free_aligned_buffer_page_end(dst_uv);                                     \
  }

TESTPTOB(TestYUY2ToNV12, YUY2ToI420, YUY2ToNV12)
TESTPTOB(TestUYVYToNV12, UYVYToI420, UYVYToNV12)

// Transitive tests.  A to B to C is same as A to C.

#define TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                       W1280, N, NEG, OFF, FMT_C, BPP_C)                      \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##_##FMT_C##N) {             \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                    \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);            \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_u, kSizeUV + OFF);                              \
    align_buffer_page_end(src_v, kSizeUV + OFF);                              \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);               \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    for (int i = 0; i < kSizeUV; ++i) {                                       \
      src_u[i + OFF] = (fastrand() & 0xff);                                   \
      src_v[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                          \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,      \
                            src_v + OFF, kStrideUV, dst_argb_b + OFF,         \
                            kStrideB, kWidth, NEG kHeight);                   \
    }                                                                         \
    /* Convert to a 3rd format in 1 step and 2 steps and compare  */          \
    const int kStrideC = kWidth * BPP_C;                                      \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);               \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);              \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                          \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                         \
    FMT_PLANAR##To##FMT_C(src_y + OFF, kWidth, src_u + OFF, kStrideUV,        \
                          src_v + OFF, kStrideUV, dst_argb_c + OFF, kStrideC, \
                          kWidth, NEG kHeight);                               \
    /* Convert B to C */                                                      \
    FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF, kStrideC, \
                     kWidth, kHeight);                                        \
    for (int i = 0; i < kStrideC * kHeight; ++i) {                            \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_bc[i + OFF]);                   \
    }                                                                         \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
    free_aligned_buffer_page_end(dst_argb_b);                                 \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_bc);                                \
  }

#define TESTPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                      FMT_C, BPP_C)                                          \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_ - 4, _Any, +, 0, FMT_C, BPP_C)             \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Unaligned, +, 1, FMT_C, BPP_C)           \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Invert, -, 0, FMT_C, BPP_C)              \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Opt, +, 0, FMT_C, BPP_C)

TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTPLANARTOE(J420, 2, 2, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(J420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, RGB24, 3)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, RAW, 3)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RAW, 3)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, RGB24, 1, 3, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, RAW, 1, 3, RGB24, 3)
TESTPLANARTOE(H420, 2, 2, RGB24, 1, 3, RAW, 3)
TESTPLANARTOE(H420, 2, 2, ARGB, 1, 4, RAW, 3)
TESTPLANARTOE(H420, 2, 2, RAW, 1, 3, ARGB, 4)
#ifdef LITTLE_ENDIAN_TEST
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RGB565, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB1555, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB4444, 2)
TESTPLANARTOE(I422, 2, 1, ARGB, 1, 4, RGB565, 2)
#endif
TESTPLANARTOE(J422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(J422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(H422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(H422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(I444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(J444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(I444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, UYVY, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, UYVY, 2, 4, ARGB, 4)

#define TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                        W1280, N, NEG, OFF, FMT_C, BPP_C, ATTEN)               \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##_##FMT_C##N) {              \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                     \
    const int kSizeUV =                                                        \
        SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y);          \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(src_a, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);                \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
      src_a[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                           \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(                                                   \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),      \
          src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X), src_a + OFF, kWidth,      \
          dst_argb_b + OFF, kStrideB, kWidth, NEG kHeight, ATTEN);             \
    }                                                                          \
    /* Convert to a 3rd format in 1 step and 2 steps and compare  */           \
    const int kStrideC = kWidth * BPP_C;                                       \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);               \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                           \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                          \
    FMT_PLANAR##To##FMT_C(                                                     \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),        \
        src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X), src_a + OFF, kWidth,        \
        dst_argb_c + OFF, kStrideC, kWidth, NEG kHeight, ATTEN);               \
    /* Convert B to C */                                                       \
    FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF, kStrideC,  \
                     kWidth, kHeight);                                         \
    for (int i = 0; i < kStrideC * kHeight; ++i) {                             \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_bc[i + OFF]);                    \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(src_a);                                       \
    free_aligned_buffer_page_end(dst_argb_b);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_bc);                                 \
  }

#define TESTQPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                       FMT_C, BPP_C)                                          \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_ - 4, _Any, +, 0, FMT_C, BPP_C, 0)          \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Unaligned, +, 1, FMT_C, BPP_C, 0)        \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Invert, -, 0, FMT_C, BPP_C, 0)           \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Opt, +, 0, FMT_C, BPP_C, 0)              \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Premult, +, 0, FMT_C, BPP_C, 1)

TESTQPLANARTOE(I420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(I420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)

#define TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, W1280, N, NEG, \
                      OFF, FMT_C, BPP_C)                                       \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##_##FMT_C##N) {                   \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                            \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideA = SUBSAMPLE(kWidth, SUB_A) * BPP_A;                     \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                     \
    align_buffer_page_end(src_argb_a, kStrideA* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);                \
    MemRandomize(src_argb_a + OFF, kStrideA * kHeight);                        \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                           \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_B(src_argb_a + OFF, kStrideA, dst_argb_b + OFF, kStrideB, \
                       kWidth, NEG kHeight);                                   \
    }                                                                          \
    /* Convert to a 3rd format in 1 step and 2 steps and compare  */           \
    const int kStrideC = kWidth * BPP_C;                                       \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);               \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                           \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                          \
    FMT_A##To##FMT_C(src_argb_a + OFF, kStrideA, dst_argb_c + OFF, kStrideC,   \
                     kWidth, NEG kHeight);                                     \
    /* Convert B to C */                                                       \
    FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF, kStrideC,  \
                     kWidth, kHeight);                                         \
    for (int i = 0; i < kStrideC * kHeight; i += 4) {                          \
      EXPECT_EQ(dst_argb_c[i + OFF + 0], dst_argb_bc[i + OFF + 0]);            \
      EXPECT_EQ(dst_argb_c[i + OFF + 1], dst_argb_bc[i + OFF + 1]);            \
      EXPECT_EQ(dst_argb_c[i + OFF + 2], dst_argb_bc[i + OFF + 2]);            \
      EXPECT_NEAR(dst_argb_c[i + OFF + 3], dst_argb_bc[i + OFF + 3], 64);      \
    }                                                                          \
    free_aligned_buffer_page_end(src_argb_a);                                  \
    free_aligned_buffer_page_end(dst_argb_b);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_bc);                                 \
  }

#define TESTPLANETOE(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, FMT_C, BPP_C) \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B,                    \
                benchmark_width_ - 4, _Any, +, 0, FMT_C, BPP_C)              \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Unaligned, +, 1, FMT_C, BPP_C)                              \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Invert, -, 0, FMT_C, BPP_C)                                 \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Opt, +, 0, FMT_C, BPP_C)

// Caveat: Destination needs to be 4 bytes
#ifdef LITTLE_ENDIAN_TEST
TESTPLANETOE(ARGB, 1, 4, AR30, 1, 4, ARGB, 4)
TESTPLANETOE(ABGR, 1, 4, AR30, 1, 4, ABGR, 4)
TESTPLANETOE(AR30, 1, 4, ARGB, 1, 4, ABGR, 4)
TESTPLANETOE(AR30, 1, 4, ABGR, 1, 4, ARGB, 4)
TESTPLANETOE(ARGB, 1, 4, AB30, 1, 4, ARGB, 4)
TESTPLANETOE(ABGR, 1, 4, AB30, 1, 4, ABGR, 4)
TESTPLANETOE(AB30, 1, 4, ARGB, 1, 4, ABGR, 4)
TESTPLANETOE(AB30, 1, 4, ABGR, 1, 4, ARGB, 4)
#endif

TEST_F(LibYUVConvertTest, RotateWithARGBSource) {
  // 2x2 frames
  uint32_t src[4];
  uint32_t dst[4];
  // some random input
  src[0] = 0x11000000;
  src[1] = 0x00450000;
  src[2] = 0x00009f00;
  src[3] = 0x000000ff;
  // zeros on destination
  dst[0] = 0x00000000;
  dst[1] = 0x00000000;
  dst[2] = 0x00000000;
  dst[3] = 0x00000000;

  int r = ConvertToARGB(reinterpret_cast<uint8_t*>(src),
                        16,  // input size
                        reinterpret_cast<uint8_t*>(dst),
                        8,  // destination stride
                        0,  // crop_x
                        0,  // crop_y
                        2,  // width
                        2,  // height
                        2,  // crop width
                        2,  // crop height
                        kRotate90, FOURCC_ARGB);

  EXPECT_EQ(r, 0);
  // 90 degrees rotation, no conversion
  EXPECT_EQ(dst[0], src[2]);
  EXPECT_EQ(dst[1], src[0]);
  EXPECT_EQ(dst[2], src[3]);
  EXPECT_EQ(dst[3], src[1]);
}

#ifdef HAS_ARGBTOAR30ROW_AVX2
TEST_F(LibYUVConvertTest, ARGBToAR30Row_Opt) {
  // ARGBToAR30Row_AVX2 expects a multiple of 8 pixels.
  const int kPixels = (benchmark_width_ * benchmark_height_ + 7) & ~7;
  align_buffer_page_end(src, kPixels * 4);
  align_buffer_page_end(dst_opt, kPixels * 4);
  align_buffer_page_end(dst_c, kPixels * 4);
  MemRandomize(src, kPixels * 4);
  memset(dst_opt, 0, kPixels * 4);
  memset(dst_c, 1, kPixels * 4);

  ARGBToAR30Row_C(src, dst_c, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      ARGBToAR30Row_AVX2(src, dst_opt, kPixels);
    } else if (has_ssse3) {
      ARGBToAR30Row_SSSE3(src, dst_opt, kPixels);
    } else {
      ARGBToAR30Row_C(src, dst_opt, kPixels);
    }
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_opt[i], dst_c[i]);
  }

  free_aligned_buffer_page_end(src);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(dst_c);
}
#endif  // HAS_ARGBTOAR30ROW_AVX2

#ifdef HAS_ABGRTOAR30ROW_AVX2
TEST_F(LibYUVConvertTest, ABGRToAR30Row_Opt) {
  // ABGRToAR30Row_AVX2 expects a multiple of 8 pixels.
  const int kPixels = (benchmark_width_ * benchmark_height_ + 7) & ~7;
  align_buffer_page_end(src, kPixels * 4);
  align_buffer_page_end(dst_opt, kPixels * 4);
  align_buffer_page_end(dst_c, kPixels * 4);
  MemRandomize(src, kPixels * 4);
  memset(dst_opt, 0, kPixels * 4);
  memset(dst_c, 1, kPixels * 4);

  ABGRToAR30Row_C(src, dst_c, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      ABGRToAR30Row_AVX2(src, dst_opt, kPixels);
    } else if (has_ssse3) {
      ABGRToAR30Row_SSSE3(src, dst_opt, kPixels);
    } else {
      ABGRToAR30Row_C(src, dst_opt, kPixels);
    }
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_opt[i], dst_c[i]);
  }

  free_aligned_buffer_page_end(src);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(dst_c);
}
#endif  // HAS_ABGRTOAR30ROW_AVX2

// TODO(fbarchard): Fix clamping issue affected by U channel.
#define TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,   \
                         ALIGN, YALIGN, W1280, DIFF, N, NEG, SOFF, DOFF)   \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                    \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                        \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);               \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                  \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                    \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);         \
    const int kBpc = 2;                                                    \
    align_buffer_page_end(src_y, kWidth* kHeight* kBpc + SOFF);            \
    align_buffer_page_end(src_u, kSizeUV* kBpc + SOFF);                    \
    align_buffer_page_end(src_v, kSizeUV* kBpc + SOFF);                    \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + DOFF);           \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + DOFF);         \
    for (int i = 0; i < kWidth * kHeight; ++i) {                           \
      reinterpret_cast<uint16_t*>(src_y + SOFF)[i] = (fastrand() & 0x3ff); \
    }                                                                      \
    for (int i = 0; i < kSizeUV; ++i) {                                    \
      reinterpret_cast<uint16_t*>(src_u + SOFF)[i] = (fastrand() & 0x3ff); \
      reinterpret_cast<uint16_t*>(src_v + SOFF)[i] = (fastrand() & 0x3ff); \
    }                                                                      \
    memset(dst_argb_c + DOFF, 1, kStrideB * kHeight);                      \
    memset(dst_argb_opt + DOFF, 101, kStrideB * kHeight);                  \
    MaskCpuFlags(disable_cpu_flags_);                                      \
    FMT_PLANAR##To##FMT_B(                                                 \
        reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth,                 \
        reinterpret_cast<uint16_t*>(src_u + SOFF), kStrideUV,              \
        reinterpret_cast<uint16_t*>(src_v + SOFF), kStrideUV,              \
        dst_argb_c + DOFF, kStrideB, kWidth, NEG kHeight);                 \
    MaskCpuFlags(benchmark_cpu_info_);                                     \
    for (int i = 0; i < benchmark_iterations_; ++i) {                      \
      FMT_PLANAR##To##FMT_B(                                               \
          reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth,               \
          reinterpret_cast<uint16_t*>(src_u + SOFF), kStrideUV,            \
          reinterpret_cast<uint16_t*>(src_v + SOFF), kStrideUV,            \
          dst_argb_opt + DOFF, kStrideB, kWidth, NEG kHeight);             \
    }                                                                      \
    int max_diff = 0;                                                      \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                   \
      int abs_diff = abs(static_cast<int>(dst_argb_c[i + DOFF]) -          \
                         static_cast<int>(dst_argb_opt[i + DOFF]));        \
      if (abs_diff > max_diff) {                                           \
        max_diff = abs_diff;                                               \
      }                                                                    \
    }                                                                      \
    EXPECT_LE(max_diff, DIFF);                                             \
    free_aligned_buffer_page_end(src_y);                                   \
    free_aligned_buffer_page_end(src_u);                                   \
    free_aligned_buffer_page_end(src_v);                                   \
    free_aligned_buffer_page_end(dst_argb_c);                              \
    free_aligned_buffer_page_end(dst_argb_opt);                            \
  }

#define TESTPLANAR16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                        YALIGN, DIFF)                                          \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                   YALIGN, benchmark_width_ - 4, DIFF, _Any, +, 0, 0)          \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                   YALIGN, benchmark_width_, DIFF, _Unaligned, +, 1, 1)        \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                   YALIGN, benchmark_width_, DIFF, _Invert, -, 0, 0)           \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                   YALIGN, benchmark_width_, DIFF, _Opt, +, 0, 0)

TESTPLANAR16TOB(I010, 2, 2, ARGB, 4, 4, 1, 2)
TESTPLANAR16TOB(I010, 2, 2, ABGR, 4, 4, 1, 2)
#ifdef LITTLE_ENDIAN_TEST
TESTPLANAR16TOB(I010, 2, 2, AR30, 4, 4, 1, 2)
TESTPLANAR16TOB(I010, 2, 2, AB30, 4, 4, 1, 2)
#endif
TESTPLANAR16TOB(H010, 2, 2, ARGB, 4, 4, 1, 2)
TESTPLANAR16TOB(H010, 2, 2, ABGR, 4, 4, 1, 2)
#ifdef LITTLE_ENDIAN_TEST
TESTPLANAR16TOB(H010, 2, 2, AR30, 4, 4, 1, 2)
TESTPLANAR16TOB(H010, 2, 2, AB30, 4, 4, 1, 2)
#endif

static int Clamp(int y) {
  if (y < 0) {
    y = 0;
  }
  if (y > 255) {
    y = 255;
  }
  return y;
}

static int Clamp10(int y) {
  if (y < 0) {
    y = 0;
  }
  if (y > 1023) {
    y = 1023;
  }
  return y;
}

// Test 8 bit YUV to 8 bit RGB
TEST_F(LibYUVConvertTest, TestH420ToARGB) {
  const int kSize = 256;
  int histogram_b[256];
  int histogram_g[256];
  int histogram_r[256];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize + kSize / 2 * 2);
  align_buffer_page_end(argb_pixels, kSize * 4);
  uint8_t* orig_y = orig_yuv;
  uint8_t* orig_u = orig_y + kSize;
  uint8_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 128;  // 128 is 0.
    orig_v[i] = 128;
  }

  H420ToARGB(orig_y, 0, orig_u, 0, orig_v, 0, argb_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b = argb_pixels[i * 4 + 0];
    int g = argb_pixels[i * 4 + 1];
    int r = argb_pixels[i * 4 + 2];
    int a = argb_pixels[i * 4 + 3];
    ++histogram_b[b];
    ++histogram_g[g];
    ++histogram_r[r];
    int expected_y = Clamp(static_cast<int>((i - 16) * 1.164f));
    EXPECT_NEAR(b, expected_y, 1);
    EXPECT_NEAR(g, expected_y, 1);
    EXPECT_NEAR(r, expected_y, 1);
    EXPECT_EQ(a, 255);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(argb_pixels);
}

// Test 10 bit YUV to 8 bit RGB
TEST_F(LibYUVConvertTest, TestH010ToARGB) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(argb_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToARGB(orig_y, 0, orig_u, 0, orig_v, 0, argb_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b = argb_pixels[i * 4 + 0];
    int g = argb_pixels[i * 4 + 1];
    int r = argb_pixels[i * 4 + 2];
    int a = argb_pixels[i * 4 + 3];
    ++histogram_b[b];
    ++histogram_g[g];
    ++histogram_r[r];
    int expected_y = Clamp(static_cast<int>((i - 64) * 1.164f / 4));
    EXPECT_NEAR(b, expected_y, 1);
    EXPECT_NEAR(g, expected_y, 1);
    EXPECT_NEAR(r, expected_y, 1);
    EXPECT_EQ(a, 255);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(argb_pixels);
}

// Test 10 bit YUV to 10 bit RGB
// Caveat: Result is near due to float rounding in expected
// result.
TEST_F(LibYUVConvertTest, TestH010ToAR30) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));

  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(ar30_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToAR30(orig_y, 0, orig_u, 0, orig_v, 0, ar30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b10 = reinterpret_cast<uint32_t*>(ar30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 10) & 1023;
    int r10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 64) * 1.164f));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ar30_pixels);
}

// Test 10 bit YUV to 10 bit RGB
// Caveat: Result is near due to float rounding in expected
// result.
TEST_F(LibYUVConvertTest, TestH010ToAB30) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));

  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(ab30_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToAB30(orig_y, 0, orig_u, 0, orig_v, 0, ab30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int r10 = reinterpret_cast<uint32_t*>(ab30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 10) & 1023;
    int b10 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 64) * 1.164f));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ab30_pixels);
}

// Test 8 bit YUV to 10 bit RGB
TEST_F(LibYUVConvertTest, TestH420ToAR30) {
  const int kSize = 256;
  const int kHistSize = 1024;
  int histogram_b[kHistSize];
  int histogram_g[kHistSize];
  int histogram_r[kHistSize];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize + kSize / 2 * 2);
  align_buffer_page_end(ar30_pixels, kSize * 4);
  uint8_t* orig_y = orig_yuv;
  uint8_t* orig_u = orig_y + kSize;
  uint8_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 128;  // 128 is 0.
    orig_v[i] = 128;
  }

  H420ToAR30(orig_y, 0, orig_u, 0, orig_v, 0, ar30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b10 = reinterpret_cast<uint32_t*>(ar30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 10) & 1023;
    int r10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 16) * 1.164f * 4.f));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kHistSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ar30_pixels);
}

// Test RGB24 to ARGB and back to RGB24
TEST_F(LibYUVConvertTest, TestARGBToRGB24) {
  const int kSize = 256;
  align_buffer_page_end(orig_rgb24, kSize * 3);
  align_buffer_page_end(argb_pixels, kSize * 4);
  align_buffer_page_end(dest_rgb24, kSize * 3);

  // Test grey scale
  for (int i = 0; i < kSize * 3; ++i) {
    orig_rgb24[i] = i;
  }

  RGB24ToARGB(orig_rgb24, 0, argb_pixels, 0, kSize, 1);
  ARGBToRGB24(argb_pixels, 0, dest_rgb24, 0, kSize, 1);

  for (int i = 0; i < kSize * 3; ++i) {
    EXPECT_EQ(orig_rgb24[i], dest_rgb24[i]);
  }

  free_aligned_buffer_page_end(orig_rgb24);
  free_aligned_buffer_page_end(argb_pixels);
  free_aligned_buffer_page_end(dest_rgb24);
}

}  // namespace libyuv
