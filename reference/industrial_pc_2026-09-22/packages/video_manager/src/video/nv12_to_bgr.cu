#include <cstdint>
#include <algorithm>
#include <cuda_runtime.h>

// 定义NV12到BGR的CUDA核函数
__global__ void nv12_to_bgr_kernel(const uint8_t *y, const uint8_t *uv,
                                   int width, int height,
                                   int y_pitch, int uv_pitch,
                                   uint8_t *bgr, int bgr_pitch) {
    // 计算当前线程的像素坐标
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y_coord = blockIdx.y * blockDim.y + threadIdx.y;

    // 检查是否超出图像边界
    if (x >= width || y_coord >= height)
        return;

    // 计算Y分量的索引
    int y_index = y_coord * y_pitch + x;

    // 计算UV分量的索引（UV分量每两个Y分量共享一个）
    int uv_index = (y_coord / 2) * uv_pitch + (x & ~1);

    // 获取Y、U、V值
    int Y = y[y_index];
    int U = uv[uv_index];
    int V = uv[uv_index + 1];

    // YUV到BGR的转换公式
    // 参考公式来源：https://en.wikipedia.org/wiki/YUV
    // R = Y + 1.402 * (V - 128)
    // G = Y - 0.344 * (U - 128) - 0.714 * (V - 128)
    // B = Y + 1.772 * (U - 128)
    int R = Y + 1.402 * (V - 128);
    int G = Y - 0.344 * (U - 128) - 0.714 * (V - 128);
    int B = Y + 1.772 * (U - 128);

    // 限制R、G、B的范围在[0, 255]
    R = min(max(R, 0), 255);
    G = min(max(G, 0), 255);
    B = min(max(B, 0), 255);

    // 计算BGR输出的索引
    int index = y_coord * bgr_pitch + x * 3;
    bgr[index + 0] = B;  // 蓝色分量
    bgr[index + 1] = G;  // 绿色分量
    bgr[index + 2] = R;  // 红色分量
}

// CUDA接口函数，调用核函数并同步设备
extern "C" void nv12_to_bgr_cuda(const uint8_t *y, const uint8_t *uv,
                                 int width, int height,
                                 int y_pitch, int uv_pitch,
                                 uint8_t *bgr, int bgr_pitch) {
    // 定义线程块大小和网格大小
    dim3 blockSize(16, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x,
                  (height + blockSize.y - 1) / blockSize.y);

    // 调用CUDA核函数
    nv12_to_bgr_kernel<<<gridSize, blockSize>>>(y, uv, width, height,
                                               y_pitch, uv_pitch,
                                               bgr, bgr_pitch);

    // 确保CUDA操作完成后再返回
    cudaDeviceSynchronize();  // 同步设备，确保所有CUDA操作执行完毕
}