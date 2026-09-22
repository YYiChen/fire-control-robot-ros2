#ifndef NV12_TO_BGR_H
#define NV12_TO_BGR_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void nv12_to_bgr_cuda(const uint8_t *y, const uint8_t *uv,
                      int width, int height,
                      int y_pitch, int uv_pitch,
                      uint8_t *bgr, int bgr_pitch);

#ifdef __cplusplus
}
#endif

#endif // NV12_TO_BGR_H