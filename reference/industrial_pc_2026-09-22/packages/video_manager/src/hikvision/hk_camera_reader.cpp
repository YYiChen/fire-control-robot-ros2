// hk_camera_reader.cpp
#include "video_manager/hikvision/hk_camera_reader.h"
#include <iostream>
#include <cstring>

int HKCameraReader::camera_count_ = 0;

HKCameraReader::HKCameraReader() : handle_(nullptr), is_opened_(false) {
    // 只有第一个实例初始化SDK
    if (camera_count_ == 0) {
        int nRet = MV_CC_Initialize();
        if (nRet != MV_OK) {
            std::cerr << "Failed to initialize HK camera SDK, error code: " << nRet << std::endl;
        }
    }
    camera_count_++;
}

HKCameraReader::~HKCameraReader() {
    release();
    camera_count_--;
    // 最后一个实例释放SDK
    if (camera_count_ == 0) {
        MV_CC_Finalize();
    }
}

bool HKCameraReader::enumDevices(MV_CC_DEVICE_INFO_LIST& stDeviceList) {
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    
    // 枚举设备
    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (nRet != MV_OK) {
        std::cerr << "Failed to enumerate devices, error code: " << nRet << std::endl;
        return false;
    }
    
    if (stDeviceList.nDeviceNum == 0) {
        std::cerr << "No device found" << std::endl;
        return false;
    }
    
    return true;
}

// 添加设备信息打印函数
void HKCameraReader::printDeviceInfo(const MV_CC_DEVICE_INFO_LIST& stDeviceList) {
    std::cout << "Found " << stDeviceList.nDeviceNum << " devices:" << std::endl;
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
        std::cout << "Device " << i << ":" << std::endl;
        
        if (pDeviceInfo->nTLayerType == MV_USB_DEVICE) {
            std::cout << "  Type: USB Camera" << std::endl;
            std::cout << "  Serial Number: " << pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber << std::endl;
            std::cout << "  Model Name: " << pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName << std::endl;
            std::cout << "  Firmware Version: " << pDeviceInfo->SpecialInfo.stUsb3VInfo.chDeviceVersion << std::endl;
        } else if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
            std::cout << "  Type: GigE Camera" << std::endl;
            std::cout << "  IP Address: " 
                      << (pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xFF) << "."
                      << ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp >> 8) & 0xFF) << "."
                      << ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp >> 16) & 0xFF) << "."
                      << ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp >> 24) & 0xFF) << std::endl;
            std::cout << "  Serial Number: " << pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber << std::endl;
            std::cout << "  Model Name: " << pDeviceInfo->SpecialInfo.stGigEInfo.chModelName << std::endl;
        }
    }
}
int HKCameraReader::selectDevice(const MV_CC_DEVICE_INFO_LIST& stDeviceList, const std::string& device_id) {
    // 打印所有找到的设备信息
    printDeviceInfo(stDeviceList);
    
    if (device_id.empty()) {
        return 0; // 默认选择第一个设备
    }
    
    // 根据设备ID选择设备
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
        char serialNumber[64] = {0};

        if (pDeviceInfo->nTLayerType == MV_USB_DEVICE) {
            // 对于USB设备，可以通过序列号匹配
            memcpy(serialNumber, pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber, 
                   sizeof(pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber));
        } else if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
            // 对于网络设备，通过序列号匹配
            memcpy(serialNumber, pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber, 
                   sizeof(pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
        }
        
        if (device_id == std::string(serialNumber)) {
            std::cout << "Found matching device with ID: " << device_id << std::endl;
            return i;
        }
    }

    std::cerr << "Device with ID " << device_id << " not found, using the first available device." << std::endl;
    return 0; // 如果找不到匹配的设备，使用默认第一个
}

bool HKCameraReader::open(const std::string& device_id) {
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    
    // 枚举设备
    if (!enumDevices(stDeviceList)) {
        return false;
    }
    
    // 选择设备
    int device_index = selectDevice(stDeviceList, device_id);
    
    // 创建设备句柄
    int nRet = MV_CC_CreateHandle(&handle_, stDeviceList.pDeviceInfo[device_index]);
    if (nRet != MV_OK) {
        std::cerr << "Failed to create device handle, error code: " << nRet << std::endl;
        return false;
    }
    
    // 打开设备
    nRet = MV_CC_OpenDevice(handle_);
    if (nRet != MV_OK) {
        std::cerr << "Failed to open device, error code: " << nRet << std::endl;
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        return false;
    }
    
    // 设置触发模式为连续触发
    nRet = MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set trigger mode, error code: " << nRet << std::endl;
    }

    // 设置采集帧率为30fps
    nRet = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", 30.0f);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set acquisition frame rate, error code: " << nRet << std::endl;
    }
    
    // 设置自动曝光为连续模式
    nRet = MV_CC_SetEnumValue(handle_, "ExposureAuto", 2); // 2表示连续自动曝光
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto exposure, error code: " << nRet << std::endl;
    }
    
    // 设置自动增益为连续模式
    nRet = MV_CC_SetEnumValue(handle_, "GainAuto", 2); // 2表示连续自动增益
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto gain, error code: " << nRet << std::endl;
    }

    // 设置自动增益上限为5
    nRet = MV_CC_SetFloatValue(handle_, "AutoGainUpperLimit", 16.0f);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto gain upper limit, error code: " << nRet << std::endl;
    }
    
    // 开始取流
    nRet = MV_CC_StartGrabbing(handle_);
    if (nRet != MV_OK) {
        std::cerr << "Failed to start grabbing, error code: " << nRet << std::endl;
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        return false;
    }
    
    is_opened_ = true;
    return true;
}

bool HKCameraReader::read(cv::Mat& frame) {
    if (!is_opened_ || !handle_) return false;
    
    MV_FRAME_OUT_INFO_EX stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    
    // 分配足够大的缓冲区 (根据实际需求调整大小)
    size_t nDataSize = 1280 * 1024 * 3;
    unsigned char* pData = new unsigned char[nDataSize];
    
    // 获取一帧图像
    int nRet = MV_CC_GetOneFrameTimeout(handle_, pData, nDataSize, &stImageInfo, 1000);
    if (nRet == MV_OK) {
        // 转换为OpenCV格式
        if (stImageInfo.enPixelType == PixelType_Gvsp_BGR8_Packed) {
            frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pData).clone();
            delete[] pData;
            return true;
        } else if (stImageInfo.enPixelType == PixelType_Gvsp_Mono8) {
            frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC1, pData).clone();
            delete[] pData;
            return true;
        } else {
            // 如果是其他格式，需要转换为BGR8
            // 这里简单处理，实际应用中可能需要更复杂的转换
            unsigned char* pImageBuffer = new unsigned char[stImageInfo.nWidth * stImageInfo.nHeight * 3];
            MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
            stConvertParam.nWidth = stImageInfo.nWidth;
            stConvertParam.nHeight = stImageInfo.nHeight;
            stConvertParam.pSrcData = pData;
            stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;
            stConvertParam.enSrcPixelType = stImageInfo.enPixelType;
            stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
            stConvertParam.pDstBuffer = pImageBuffer;
            stConvertParam.nDstBufferSize = stImageInfo.nWidth * stImageInfo.nHeight * 3;
            
            nRet = MV_CC_ConvertPixelType(handle_, &stConvertParam);
            if (MV_OK == nRet) {
                frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pImageBuffer).clone();
                delete[] pData;
                delete[] pImageBuffer;
                return true;
            }
            delete[] pImageBuffer;
        }
    }
    
    delete[] pData;
    return false;
}

bool HKCameraReader::triggerOnce(cv::Mat& frame) {
    if (!is_opened_ || !handle_) return false;
    
    // 设置为单次触发模式
    if (!setTriggerMode(1)) return false;
    
    // 发送软触发信号
    int nRet = MV_CC_SetCommandValue(handle_, "TriggerSoftware");
    if (nRet != MV_OK) {
        std::cerr << "Failed to send software trigger, error code: " << nRet << std::endl;
        setTriggerMode(0); // 恢复连续模式
        return false;
    }
    
    // 获取触发后的图像
    MV_FRAME_OUT_INFO_EX stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    
    size_t nDataSize = 1280 * 1024 * 3;
    unsigned char* pData = new unsigned char[nDataSize];
    
    // 等待触发的图像（增加超时时间）
    nRet = MV_CC_GetOneFrameTimeout(handle_, pData, nDataSize, &stImageInfo, 2000);
    bool result = false;
    
    if (nRet == MV_OK) {
        // 转换为OpenCV格式
        if (stImageInfo.enPixelType == PixelType_Gvsp_BGR8_Packed) {
            frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pData).clone();
            result = true;
        } else if (stImageInfo.enPixelType == PixelType_Gvsp_Mono8) {
            frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC1, pData).clone();
            result = true;
        } else {
            // 格式转换处理...
            unsigned char* pImageBuffer = new unsigned char[stImageInfo.nWidth * stImageInfo.nHeight * 3];
            MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
            stConvertParam.nWidth = stImageInfo.nWidth;
            stConvertParam.nHeight = stImageInfo.nHeight;
            stConvertParam.pSrcData = pData;
            stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;
            stConvertParam.enSrcPixelType = stImageInfo.enPixelType;
            stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
            stConvertParam.pDstBuffer = pImageBuffer;
            stConvertParam.nDstBufferSize = stImageInfo.nWidth * stImageInfo.nHeight * 3;
            
            nRet = MV_CC_ConvertPixelType(handle_, &stConvertParam);
            if (MV_OK == nRet) {
                frame = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pImageBuffer).clone();
                result = true;
            }
            delete[] pImageBuffer;
        }
    }
    
    delete[] pData;
    
    // 恢复连续触发模式
    setTriggerMode(0);
    
    return result;
}

void HKCameraReader::release() {
    if (handle_) {
        MV_CC_StopGrabbing(handle_);
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }
    is_opened_ = false;
}

bool HKCameraReader::setTriggerMode(int mode) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet = MV_CC_SetEnumValue(handle_, "TriggerMode", mode);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set trigger mode, error code: " << nRet << std::endl;
        return false;
    }
    return true;
}


// 设置自动增益模式
bool HKCameraReader::setAutoGainEnabled(bool enabled) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet;
    if (enabled) {
        // 启用自动增益 (连续模式)
        nRet = MV_CC_SetEnumValue(handle_, "GainAuto", 2);
    } else {
        // 禁用自动增益 (手动模式)
        nRet = MV_CC_SetEnumValue(handle_, "GainAuto", 0);
    }
    
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto gain mode, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}

// 设置手动增益值
bool HKCameraReader::setManualGain(float gain_value) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet = MV_CC_SetFloatValue(handle_, "Gain", gain_value);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set manual gain value, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}

// 设置自动增益的上下限
bool HKCameraReader::setAutoGainLimits(float lower_limit, float upper_limit) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet = MV_CC_SetFloatValue(handle_, "AutoGainLowerLimit", lower_limit);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto gain lower limit, error code: " << nRet << std::endl;
        return false;
    }
    
    nRet = MV_CC_SetFloatValue(handle_, "AutoGainUpperLimit", upper_limit);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto gain upper limit, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}

// 设置自动曝光模式
bool HKCameraReader::setAutoExposureEnabled(bool enabled) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet;
    if (enabled) {
        // 启用自动曝光 (连续模式)
        nRet = MV_CC_SetEnumValue(handle_, "ExposureAuto", 2);
    } else {
        // 禁用自动曝光 (手动模式)
        nRet = MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
    }
    
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto exposure mode, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}

// 设置曝光时间
bool HKCameraReader::setExposureTime(float exposure_time) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet = MV_CC_SetFloatValue(handle_, "ExposureTime", exposure_time);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set exposure time, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}

// 设置自动曝光时间上下限
bool HKCameraReader::setAutoExposureLimits(float lower_limit, float upper_limit) {
    if (!is_opened_ || !handle_) return false;
    
    int nRet = MV_CC_SetFloatValue(handle_, "AutoExposureTimeLowerLimit", lower_limit);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto exposure lower limit, error code: " << nRet << std::endl;
        return false;
    }
    
    nRet = MV_CC_SetFloatValue(handle_, "AutoExposureTimeUpperLimit", upper_limit);
    if (nRet != MV_OK) {
        std::cerr << "Failed to set auto exposure upper limit, error code: " << nRet << std::endl;
        return false;
    }
    
    return true;
}






