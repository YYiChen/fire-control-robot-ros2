#include <video_manager/video/rtsp_streamer.h>
#include <stdexcept>
#include <libavcodec/avcodec.h>

// 构造函数：初始化 RTSP 流媒体组件
// 参数：
//   rtspUrl: RTSP URL 地址
//   width, height: 视频帧的宽度和高度
//   fps: 帧率
RtspStreamer::RtspStreamer(const char* rtspUrl, int width, int height, int fps) 
    : outputFormatContext(nullptr), videoStream(nullptr), swsContext(nullptr), frameCounter(0) {
    // 调用 initializeOutput 方法进行初始化
    // 如果初始化失败，抛出异常
    if (initializeOutput(rtspUrl, width, height, fps) < 0) {
        throw std::runtime_error("Failed to initialize RTSP streamer");
    }
}

RtspStreamer::~RtspStreamer() {
    // 释放 SWS 上下文
    if (swsContext) {
        sws_freeContext(swsContext);
    }
    // 写入尾部信息并关闭输出上下文
    if (outputFormatContext) {
        av_write_trailer(outputFormatContext);
        avio_closep(&outputFormatContext->pb);
        avformat_free_context(outputFormatContext);
    }
}

int RtspStreamer::pushFrame(AVFrame* frame) {
    // 检查输出上下文和视频流是否有效
    if (!outputFormatContext || !videoStream || !codec_ctx) {
        return -1;
    }

    // 设置帧的时间戳
    frame->pts = av_rescale_q(frameCounter++, codec_ctx->time_base, videoStream->time_base);

    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // 接收编码后的包并写入输出
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt_h264);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            avcodec_free_context(&codec_ctx);
            return -1;
        }

        pkt_h264->stream_index = videoStream->index;
        ret = av_interleaved_write_frame(outputFormatContext, pkt_h264);
        av_packet_unref(pkt_h264);
        if (ret < 0) {
            avcodec_free_context(&codec_ctx);
            return -1;
        }
    }

    return 0;
}

int RtspStreamer::pushMatFrame(const cv::Mat& matFrame) {
    AVFrame* frame = convertMatToFrame(matFrame);   // 将 OpenCV Mat 转换为 AVFrame
    if (!frame) {
        return -1;
    }

    // 推送 AVFrame
    int ret = pushFrame(frame);
    // 释放 AVFrame
    av_frame_free(&frame);
    return ret;
}

int RtspStreamer::initializeOutput(const char* rtspUrl, int width, int height, int fps) {

    // 创建输出上下文
    if (avformat_alloc_output_context2(&outputFormatContext, NULL, "RTSP", rtspUrl) < 0){
        std::cerr << "Fail: avformat_alloc_output_context2" << std::endl;
        return -1;
    }

    // 使用tcp协议传输
    av_opt_set(outputFormatContext->priv_data, "rtsp_transport", "tcp", 0);
    // 禁用缓冲 & 低延迟模式
    av_opt_set(outputFormatContext->priv_data, "max_delay", "0", 0);
    av_opt_set(outputFormatContext->priv_data, "fflags", "nobuffer", 0); 
    av_opt_set(outputFormatContext->priv_data, "flags", "low_delay", 0);

    // 检查所有流是否都有数据，如果没有数据会等待max_interleave_delta微秒
    outputFormatContext->max_interleave_delta = 1000000;
    // 减小分析时间
    outputFormatContext->probesize = 32;
    outputFormatContext->max_analyze_duration = 0;

    // 获取编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    // const AVCodec *codec = avcodec_find_encoder_by_name ("nvenc_h264"); // 或者 h264_nvenc

    // 创建新的视频流
    videoStream = avformat_new_stream(outputFormatContext, codec);
    if (!videoStream) {
        std::cerr << "Failed to create new video stream" << std::endl;
        return -1;
    }

    // 设置时间基
    videoStream->time_base = {1, fps};
    videoStream->r_frame_rate = {fps, 1};
    videoStream->avg_frame_rate = {fps, 1};

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Failed to allocate video codec context" << std::endl;
        return -1;
    }

    // 设置编码器上下文参数
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->time_base = {1, fps};  // 确保 time_base 被正确设置
    codec_ctx->gop_size = 5;   //图像组两个关键帧（I帧）的距离
    codec_ctx->max_b_frames = 0;
    // codec_ctx->bit_rate = 1000000; // 1Mbps
    // codec_ctx->rc_max_rate = 1000000; // 最大码率
    // codec_ctx->rc_buffer_size = 2000000; // 缓冲区大小
    codec_ctx->framerate = {fps, 1};  // 添加 framerate 设置

    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;   //添加PPS、SPS

    av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);    //快速编码，但会损失质量
    // av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);  //适用于快速编码和低延迟流式传输,但是会出现绿屏
    // av_opt_set(codec_ctx->priv_data, "crf", "10", 0); // CRF 控制画质
    // av_opt_set(codec_ctx->priv_data, "x264opts", "crf=26:vbv-maxrate=728:vbv-bufsize=3640:keyint=25", 0);

    // 打开编码器
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return -1;
    }

    // 将编码器参数复制到视频流
    if (avcodec_parameters_from_context(videoStream->codecpar, codec_ctx) < 0) {
        std::cerr << "Failed to copy codec parameters" << std::endl;
        return -1;
    }

    // 写入头部信息
    int ret = avformat_write_header(outputFormatContext, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to write header: " << errbuf << std::endl;
        return -1;
    }

    //用于接收编码好的H264
    pkt_h264 = av_packet_alloc();

    av_dump_format(outputFormatContext, 0, rtspUrl, 1);

    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        //打开输出URL（Open output URL）
        if (avio_open(&outputFormatContext->pb, rtspUrl, AVIO_FLAG_WRITE) < 0) {
            printf("Fail: avio_open('%s')\n", rtspUrl);
            return -1;
        }
    }

    return 0;
}


AVFrame* RtspStreamer::convertMatToFrame(const cv::Mat& matFrame) {
    // 分配 AVFrame
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return nullptr;
    }

    // 设置帧格式、宽度和高度
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = matFrame.cols;
    frame->height = matFrame.rows;

    // 分配帧缓冲区
    if (av_frame_get_buffer(frame, 32) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }

    // 如果尚未创建 SWS 上下文，则创建
    if (!swsContext) {
        swsContext = sws_getContext(
            matFrame.cols, matFrame.rows, AV_PIX_FMT_BGR24,
            matFrame.cols, matFrame.rows, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!swsContext) {
            av_frame_free(&frame);
            return nullptr;
        }
    }

    // 使用 SWS 上下文进行颜色空间转换
    int srcLinesize[3] = {static_cast<int>(matFrame.step), 0, 0};
    sws_scale(swsContext, &matFrame.data, srcLinesize, 0, matFrame.rows, frame->data, frame->linesize);

    return frame;
}