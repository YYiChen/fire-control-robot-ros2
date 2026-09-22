#include <video_manager/video/videoreader.h>
#include <video_manager/video/nv12_to_bgr.h>
#include <iostream>
#include <chrono>
// #include <cuda_runtime.h>

using namespace std;
using namespace std::chrono;

// 解码单帧
int VideoReader::decode(AVCodecContext* codecCtx, AVFrame* frame, int* gotFrame, AVPacket* pkt) {
    int ret;
    *gotFrame = 0;

    if (pkt) {
        // 发送AVPacket给解码器
        ret = avcodec_send_packet(codecCtx, pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            return ret;
        }
    }

    // 接收AVFrame
    ret = avcodec_receive_frame(codecCtx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return ret;
    }

    if (ret >= 0) {
        *gotFrame = 1;
    }

    return 0;
}


bool VideoReader::initializeDecoder() {
    const AVCodec* codec = avcodec_find_decoder(videoStream_->codecpar->codec_id);
    if (!codec) {
        cerr << "无法找到解码器: " << avcodec_get_name(videoStream_->codecpar->codec_id) << endl;
        return false;
    }

    // 创建解码器上下文
    videoCodecContext_ = avcodec_alloc_context3(codec);
    if (!videoCodecContext_) {
        cerr << "无法分配解码器上下文" << endl;
        return false;
    }

    // 从AVStream中获取解码器参数
    if (avcodec_parameters_to_context(videoCodecContext_, videoStream_->codecpar) < 0) {
        cerr << "无法从参数创建解码器上下文" << endl;
        return false;
    }

    // 设置硬件加速为 CUDA
    // AVBufferRef* hw_device_ctx = nullptr;
    // if (hw_device_ctx == nullptr && codec->capabilities & AV_CODEC_CAP_HARDWARE) {
    //     if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, NULL, NULL, 0) < 0) {
    //         cerr << "无法创建 CUDA 硬件设备上下文" << endl;
    //         // 可降级到软件解码
    //     } else {
    //         videoCodecContext_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    //         videoCodecContext_->pix_fmt = AV_PIX_FMT_CUDA; // 使用 CUDA 显存存储
    //     }
    // }

    // 初始化解码器
    if (avcodec_open2(videoCodecContext_, codec, nullptr) < 0) {
        cerr << "无法打开解码器" << endl;
        return false;
    }

    timeBase_ = videoStream_->time_base;
    return true;
}

bool VideoReader::allocateResources() {

    // 创建缩放上下文
    scalerContext_ = sws_getCachedContext(
        scalerContext_,
        videoStream_->codecpar->width,    // 使用实际解码尺寸
        videoStream_->codecpar->height,
        videoCodecContext_->pix_fmt,
        videoStream_->codecpar->width,
        videoStream_->codecpar->height,
        AV_PIX_FMT_BGR24,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr
    );

    if (!scalerContext_) {
        cerr << "无法创建缩放上下文" << endl;
        return false;
    }

    // 记录实际解码尺寸
    width_ = videoStream_->codecpar->width;
    height_ = videoStream_->codecpar->height;
    frame_number_ = videoStream_->nb_frames;
    //FPS 的值不是固定的 , 随着视频播放 , 其帧率也会随之改变
    // frameRate_ = videoStream_->avg_frame_rate.num / videoStream_->avg_frame_rate.den;
    frameRate_ = av_q2d(videoStream_->avg_frame_rate);
    duration_ = formatContext_ ? formatContext_->duration : 0;

    decodedFrame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    // 初始化图像数据存储空间
    convertedFrame_ = av_frame_alloc();
    framebuf = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGR24, width_, height_, 1) * sizeof(uint8_t));
    av_image_fill_arrays(convertedFrame_->data, convertedFrame_->linesize, framebuf, AV_PIX_FMT_BGR24, width_, height_, 1);

    return true;
}

void VideoReader::freeResources() {
    av_frame_free(&decodedFrame_);
    av_frame_free(&convertedFrame_);
    av_packet_free(&packet_);
    sws_freeContext(scalerContext_);
    avcodec_free_context(&videoCodecContext_);
    avformat_close_input(&formatContext_);
    av_dict_free(&formatOptions_);
    av_free(framebuf);
}

VideoReader::VideoReader() {
    
}

VideoReader::VideoReader(const string& input) {
    open(input);
}

VideoReader::~VideoReader() {
    close();
}

bool VideoReader::open(const string& input) {
    if (!openInput(input)) return false;
    if (!findVideoStream()) return false;
    if (!initializeDecoder()) return false;
    if (!allocateResources()) return false;

    isOpened_ = true;
    return true;
}

bool VideoReader::open(int device) {
    // TODO: 实现USB摄像头支持
    return false;
}

bool VideoReader::read(cv::Mat& mat) {
    int gotFrame = 0;
    int ret;
    auto start = std::chrono::steady_clock::now();

    while (!endOfStream_) {
        // 读取一帧数据
        ret = av_read_frame(formatContext_, packet_);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                endOfStream_ = true;
                av_packet_unref(packet_);
                continue;
            }
            cerr << "读取帧失败" << endl;
            return false;
        }

        if (packet_->stream_index != streamIndex_) {
            av_packet_unref(packet_);
            continue;
        }

        ret = decode(videoCodecContext_, decodedFrame_, &gotFrame, packet_);
        av_packet_unref(packet_);

        if (ret < 0) {
            cerr << "解码失败" << endl;
            return false;
        }

        if (gotFrame && decodedFrame_->data[0]) {
            // 转换为BGR24
            sws_scale(
                scalerContext_,
                (const unsigned char* const*)decodedFrame_->data,
                decodedFrame_->linesize,
                0,
                decodedFrame_->height,
                convertedFrame_->data,
                convertedFrame_->linesize
            );

            // 获取当前播放时间(s)
            timestamp_ = decodedFrame_->best_effort_timestamp * av_q2d(videoStream_->time_base);
            double extra_delay = decodedFrame_->repeat_pict / ( frameRate_ * 2 );
            double frame_delay = 1.0 / frameRate_;
            //计算总的帧间隔时间 , 这是真实的间隔时间
            double total_frame_delay = frame_delay + extra_delay;

            AVFrame* cpu_frame = nullptr;
            bool need_free_cpu_frame = false;

            // 使用 CUDA 加速转换 NV12 -> BGR24
            // nv12_to_bgr_cuda(
            //     decodedFrame_->data[0],         // Y plane
            //     decodedFrame_->data[1],         // UV plane
            //     width_,                         // 宽度
            //     height_,                        // 高度
            //     decodedFrame_->linesize[0],     // Y pitch
            //     decodedFrame_->linesize[1],     // UV pitch
            //     framebuf,                       // 输出内存 buffer
            //     convertedFrame_->linesize[0]    // 输出步长
            // );

            // cudaError_t cudaStatus = cudaGetLastError();
            // if (cudaStatus != cudaSuccess) {
            //     std::cerr << "CUDA error: " << cudaGetErrorString(cudaStatus) << std::endl;
            //     return false;
            // }

            // 使用正确的步长参数构造Mat
            mat = cv::Mat(height_, width_, CV_8UC3, framebuf, convertedFrame_->linesize[0]);
            // 打印第一个像素值用于调试
            // std::cout << "First pixel: "
            //         << (int)framebuf[0] << ", "
            //         << (int)framebuf[1] << ", "
            //         << (int)framebuf[2] << std::endl;
            auto end = std::chrono::steady_clock::now();
            auto decode_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            int64_t delay_us = static_cast<int64_t>(total_frame_delay * 1e6) - decode_time_us;
            microsecondsDelay_ = std::max<int64_t>(0, delay_us);    // 避免负值
            
            av_frame_unref(decodedFrame_);
            return true;
        }
    }

    return false;
}

bool VideoReader::seekToProgress(double progress) {
    if (!formatContext_ || streamIndex_ < 0) return false;

    int64_t seekPos = static_cast<int64_t>(progress * av_q2d(timeBase_) * AV_TIME_BASE);
    if (av_seek_frame(formatContext_, streamIndex_, seekPos, AVSEEK_FLAG_BACKWARD) < 0) {
        cerr << "跳转失败" << endl;
        return false;
    }

    avcodec_flush_buffers(videoCodecContext_);
    endOfStream_ = false;
    return true;
}

// 获取帧率
double VideoReader::getFrameRate() const {
    
    return frameRate_;
}
double VideoReader::getDurationInSeconds() const {
    return duration_ / (double)AV_TIME_BASE;
}

double VideoReader::getTimestamp() const {
    return timestamp_;
}

int VideoReader::getFrameDelay() const {
    return microsecondsDelay_;
}

bool VideoReader::isOpened() const {
    return isOpened_;
}

void VideoReader::close() {
    freeResources();
    isOpened_ = false;
    endOfStream_ = false;
}

bool VideoReader::openInput(const string& input) {
    int ret;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "stimeout", "2000000", 0); // 2秒超时
    av_dict_set(&opts, "rtsp_transport", "tcp", 0); // 使用TCP传输
    av_dict_set(&formatOptions_, "fflags", "nobuffer", 0);     // 不缓存数据
    av_dict_set(&formatOptions_, "flags", "low_delay", 0);     // 启用低延迟标志
    av_dict_set(&formatOptions_, "probesize", "32", 0);         // 缩短探测大小
    av_dict_set(&formatOptions_, "analyzeduration", "0", 0); 

    // 使用avformat_open_input函数打开输入流
    ret = avformat_open_input(&formatContext_, input.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        cerr << "无法打开输入流: " << input << " (错误码: " << ret << ")" << endl;
        return false;
    }

    // 使用avformat_find_stream_info函数获取流信息
    ret = avformat_find_stream_info(formatContext_, nullptr);
    if (ret < 0) {
        cerr << "无法获取流信息 (错误码: " << ret << ")" << endl;
        return false;
    }

    return true;
}

bool VideoReader::findVideoStream() {
    AVCodec* codec = nullptr;

    // 寻找视频流
    int ret = av_find_best_stream(formatContext_, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (ret < 0) {
        cerr << "未找到视频流" << endl;
        return false;
    }

    streamIndex_ = ret;
    videoStream_ = formatContext_->streams[streamIndex_];
    return true;
}
