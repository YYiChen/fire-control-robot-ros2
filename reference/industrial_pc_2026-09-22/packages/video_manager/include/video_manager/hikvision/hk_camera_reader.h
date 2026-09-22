// hk_camera_reader.h
#ifndef HK_CAMERA_READER_H
#define HK_CAMERA_READER_H

#include <opencv2/opencv.hpp>
#include <string>
#include "MvCameraControl.h"

class HKCameraReader {
public:
    HKCameraReader();
    ~HKCameraReader();
    
    bool open(const std::string& device_id = "");
    bool read(cv::Mat& frame);
    void release();

    bool triggerOnce(cv::Mat& frame);
    bool setTriggerMode(int mode); // 0:连续触发, 1:单次触发
    bool setAutoGainEnabled(bool enabled);
    bool setManualGain(float gain_value);
    bool setAutoGainLimits(float lower_limit, float upper_limit);
    bool setAutoExposureEnabled(bool enabled);
    bool setExposureTime(float exposure_time);
    bool setAutoExposureLimits(float lower_limit, float upper_limit);
    bool isOpened() const { return is_opened_; }

private:
    void* handle_;
    bool is_opened_;
    static int camera_count_;
    
    void printDeviceInfo(const MV_CC_DEVICE_INFO_LIST& stDeviceList);
    bool enumDevices(MV_CC_DEVICE_INFO_LIST& stDeviceList);
    int selectDevice(const MV_CC_DEVICE_INFO_LIST& stDeviceList, const std::string& device_id);
};

#endif // HK_CAMERA_READER_H