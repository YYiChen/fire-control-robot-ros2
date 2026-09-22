#ifndef RTSP_STREAMER_H
#define RTSP_STREAMER_H

#include <opencv2/opencv.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
}

class RtspStreamer {
public:
    RtspStreamer(const char* rtspUrl, int width, int height, int fps);
    ~RtspStreamer();

    // 推流 AVFrame 帧
    int pushFrame(AVFrame* frame);

    // 推流 cv::Mat 数据
    int pushMatFrame(const cv::Mat& matFrame);

private:
    AVFormatContext* outputFormatContext;
    AVCodecContext *codec_ctx;
    AVStream *videoStream;
    AVPacket *pkt_h264;
    SwsContext* swsContext;
    int frameCounter;

    int initializeOutput(const char* rtspUrl, int width, int height, int fps);
    AVFrame* convertMatToFrame(const cv::Mat& matFrame);
};

#endif // RTSP_STREAMER_H