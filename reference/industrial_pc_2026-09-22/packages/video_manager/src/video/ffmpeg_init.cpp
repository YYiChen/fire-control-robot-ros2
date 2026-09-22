// ffmpeg_init.cpp
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

namespace {

struct FFmpegInitializer {
    FFmpegInitializer() {
        avdevice_register_all();
        avformat_network_init();
    }
};

// 静态变量保证只初始化一次
static const FFmpegInitializer init;
} // namespace