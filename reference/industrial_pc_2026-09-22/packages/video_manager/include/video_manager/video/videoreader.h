/**
 * @file videoreader.hpp
 * @brief 提供从视频文件、RTSP流、USB摄像头等源读取视频帧的功能。
 * 支持FFmpeg解码并转换为 OpenCV Mat格式，支持seek操作和元数据解析。
 */
#ifndef VIDEOREADER_HPP
#define VIDEOREADER_HPP

// FFMPEG 头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}
#include <opencv2/core.hpp> // OpenCV 核心库
#include <string>
#include <memory>

using namespace std;

class VideoReader {
private:
    // FFmpeg上下文及资源
    AVFormatContext* formatContext_{nullptr};   // 输入格式上下文
    AVCodecContext* videoCodecContext_{nullptr};    // 视频解码器上下文
    AVStream* videoStream_{nullptr};    // 视频流
    SwsContext* scalerContext_{nullptr};    // 缩放器上下文
    AVFrame* decodedFrame_{nullptr};    // 解码后的帧
    AVFrame* convertedFrame_{nullptr};  // 转换后的帧
    uint8_t *framebuf;                  // 帧缓冲区
    AVPacket* packet_{nullptr};         // 输入数据包
    AVDictionary* formatOptions_{nullptr};  // 输入格式选项

    // 视频属性
    int streamIndex_{-1};   // 视频流索引
    int width_{0};          // 视频宽度
    int height_{0};         // 视频高度
    int frameRate_{0};      // 帧率
    int frame_number_{0};
    int64_t duration_{0};   // 视频时长（毫秒）
    AVRational timeBase_{}; // 时间基
    double timestamp_{0.0}; // 当前帧时间戳
    int microsecondsDelay_{0};  // 帧延迟（微秒）

    // 状态标志
    bool isOpened_{false};  // 视频是否已打开
    bool endOfStream_{false};   // 是否到达流末尾

    // 私有方法

    /**
     * @brief 打开输入源。
     * @param input 输入路径（文件、RTSP等）
     * @return 是否成功打开
     */
    bool openInput(const string& input);

    /**
     * @brief 查找视频流。
     * @return 是否找到视频流
     */
    bool findVideoStream();

    /**
     * @brief 初始化视频解码器。
     * @return 是否成功初始化
     */
    bool initializeDecoder();

    /**
     * @brief 分配内部资源。
     * @return 是否成功分配
     */
    bool allocateResources();

    /**
     * @brief 释放内部资源。
     */
    void freeResources();

    /**
     * @brief 解码视频帧。
     * @param avctx 视频解码器上下文
     * @param frame 解码后的帧
     * @param gotFrame 是否成功解码
     * @param pkt 输入数据包
     * @return 解码结果
     */
    int decode(AVCodecContext* codecCtx, AVFrame* frame, int* gotFrame, AVPacket* pkt); 

public:
    /**
     * @brief 默认构造函数，初始化内部FFmpeg组件。
     */
    VideoReader();

    /**
     * @brief 构造并立即打开指定输入源。
     * @param input 输入路径（文件、RTSP等）
     */
    explicit VideoReader(const string& input);

    /**
     * @brief 析构函数，释放所有资源。
     */
    ~VideoReader();

    /**
     * @brief 打开视频文件或流。
     * @param input 输入路径（文件、RTSP等）
     * @return 是否成功打开
     */
    bool open(const string& input);

    /**
     * @brief 打开设备（如USB摄像头）。
     * @param device 设备索引
     * @return 是否成功打开
     */
    bool open(int device);

    /**
     * @brief 读取下一帧并转换为OpenCV Mat格式。
     * @param mat 输出的Mat对象
     * @return 是否成功读取帧
     */
    bool read(cv::Mat& mat);

    /**
     * @brief 跳转到指定进度（基于总帧数比例）。
     * @param progress 进度比例 [0.0, 1.0]
     * @return 是否成功跳转
     */
    bool seekToProgress(double progress);
    /**
     * @brief 获取视频帧率。
     * @return 视频帧率
     */
    double getFrameRate() const;

    /**
     * @brief 获取当前视频总时长（秒）。
     * @return 视频时长（秒）
     */
    double getDurationInSeconds() const;

    /**
     * @brief 获取当前帧的时间戳（微秒）。
     * @return 时间戳
     */
    double getTimestamp() const;

    /**
     * @brief 获取当前帧延迟（微秒）。
     * @return 延迟时间
     */
    int getFrameDelay() const;

    /**
     * @brief 检查是否为QYSEA设备。
     * @return 是否为QYSEA
     */
    bool isQyseaDevice() const;

    /**
     * @brief 检查视频是否已打开。
     * @return 是否已打开
     */
    bool isOpened() const;

    /**
     * @brief 关闭当前视频源。
     */
    void close();
};

#endif // VIDEOREADER_HPP