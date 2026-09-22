#include <rclcpp/rclcpp.hpp>
#include "common_interfaces/srv/lift_command.hpp"
#include "common_interfaces/msg/lift_status.hpp"
#include "common_interfaces/msg/lift_query.hpp"
#include <libserial/SerialPort.h>
#include <libserial/SerialStream.h>
#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>
#include <libudev.h>
#include <queue>
#include <mutex>

using namespace std::chrono_literals;
using LiftCommand = common_interfaces::srv::LiftCommand;
using Request = std::shared_ptr<LiftCommand::Request>;
using Response = std::shared_ptr<LiftCommand::Response>;
using LiftStatus = common_interfaces::msg::LiftStatus;
using LiftQuery = common_interfaces::msg::LiftQuery;

class LiftController : public rclcpp::Node
{
public:
  LiftController() : Node("lift_controller")
  {
    autoDetectPort();
    // 参数初始化
    if (port_name_.empty()) {
        this->declare_parameter("port", "/dev/ttyUSB0");
        port_name_ = this->get_parameter("port").as_string();
    }
    this->declare_parameter("timeout_ms", 500);
    this->declare_parameter("default_device_id", 1);
    this->declare_parameter("step_size", 1);

    // 获取参数
    timeout_ms_ = this->get_parameter("timeout_ms").as_int();
    device_id_ = this->get_parameter("default_device_id").as_int();
    step_size_ = static_cast<uint16_t>(this->get_parameter("step_size").as_int());

    // 通信接口初始化
    try {
      serial_port_.Open(port_name_);
      serial_port_.SetBaudRate(LibSerial::BaudRate::BAUD_9600);
      serial_port_.SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8);
      serial_port_.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
      serial_port_.SetFlowControl(LibSerial::FlowControl::FLOW_CONTROL_NONE);
      RCLCPP_INFO(this->get_logger(), "Serial port opened successfully");
    } catch (const LibSerial::OpenFailed&) {
      RCLCPP_FATAL(this->get_logger(), "Failed to open serial port %s", port_name_.c_str());
      rclcpp::shutdown();
      return;
    }

    // 创建服务
    command_srv_ = this->create_service<LiftCommand>(
        "lift/command",
        [this](const Request& req, const Response& res)
        {
            handleCommand(req, res);
        });

    // 创建发布者/订阅者
    query_sub_ = this->create_subscription<LiftQuery>(
        "lift/query", 10,
        std::bind(&LiftController::queryCallback, this, std::placeholders::_1));

    status_pub_ = this->create_publisher<LiftStatus>("lift/status", 10);
    query_pub_ = this->create_publisher<LiftQuery>("lift/query", 10);

    // 状态查询定时器
    status_timer_ = this->create_wall_timer(
      500ms, std::bind(&LiftController::statusTimerCallback, this));

    // 启动串口读取线程
    serial_thread_ = std::thread(&LiftController::serialReadThread, this);
  }

  ~LiftController()
  {
    if (serial_thread_.joinable())
    {
      terminate_thread_ = true;
      serial_thread_.join();
    }
    if (serial_port_.IsOpen())
    {
      serial_port_.Close();
    }
  }

private:
  // 枚举系统中的串口设备
  std::vector<std::string> enumerateSerialDevices() 
  {
      std::vector<std::string> ports;
      udev *udev = udev_new();
      udev_enumerate *enumerate = udev_enumerate_new(udev);
      
      // 过滤串口设备
      udev_enumerate_add_match_subsystem(enumerate, "tty");
      udev_enumerate_scan_devices(enumerate);
      
      udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
      udev_list_entry *entry;
      
      udev_list_entry_foreach(entry, devices) {
          const char *path = udev_list_entry_get_name(entry);
          udev_device *dev = udev_device_new_from_syspath(udev, path);
          
          // 获取设备节点路径（如 /dev/ttyUSB0）
          const char *devnode = udev_device_get_devnode(dev);
          if (devnode) {
              ports.push_back(devnode);
          }
          udev_device_unref(dev);
      }
      
      udev_enumerate_unref(enumerate);
      udev_unref(udev);
      return ports;
  }
  
  // 自动检测串口设备
  void autoDetectPort() 
  {
    auto ports = enumerateSerialDevices(); // 或 findSerialPorts()
    if (!ports.empty()) {
        port_name_ = ports[0]; // 默认选择第一个找到的设备
        RCLCPP_INFO(this->get_logger(), "Auto detected port: %s", port_name_.c_str());
    } else{
        RCLCPP_ERROR(this->get_logger(), "No serial port detected");
    }
  }
  
  // 创建高度设置帧
  std::vector<uint8_t> createHeightCommandFrame(uint16_t target_height_mm) 
  {
    std::vector<uint8_t> frame = {0x01, 0x06, 0x00, 0x02};
    
    // 添加高度数据
    target_height_mm = std::clamp(target_height_mm, static_cast<uint16_t>(1), static_cast<uint16_t>(380));
    frame.push_back(static_cast<uint8_t>(target_height_mm >> 8));   // 高字节
    frame.push_back(static_cast<uint8_t>(target_height_mm & 0xFF)); // 低字节

    // 计算CRC
    uint16_t crc = calculateCRC(frame);
    frame.push_back(static_cast<uint8_t>(crc & 0xFF)); // CRC低字节
    frame.push_back(static_cast<uint8_t>(crc >> 8));   // CRC高字节

    return frame;
  }
  
  // 协议转换核心函数
  std::vector<uint8_t> createCommandFrame(const Request& req)
  {
    std::vector<uint8_t> frame;
    
    switch (req->command_type)
    {
    case LiftCommand::Request::STOP:
      return {0x01, 0x06, 0x00, 0x01, 0x00, 0x01, 0x19, 0xCA};

    case LiftCommand::Request::UP:
      return {0x01, 0x06, 0x00, 0x01, 0x00, 0x02, 0x59, 0xCB};

    case LiftCommand::Request::DOWN:
      return {0x01, 0x06, 0x00, 0x01, 0x00, 0x04, 0xD9, 0xC9};

    case LiftCommand::Request::RESET:
      return {0x01, 0x06, 0x00, 0x01, 0x00, 0x08, 0xD9, 0xCC};

    case LiftCommand::Request::SET_HEIGHT:
      return createHeightCommandFrame(req->target_height);
    case LiftCommand::Request::STEP_UP:
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      return createHeightCommandFrame(static_cast<uint16_t>(current_height_mm + step_size_));
    }
    case LiftCommand::Request::STEP_DOWN:
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      return createHeightCommandFrame(static_cast<uint16_t>(current_height_mm - step_size_));
    }
        
    case LiftCommand::Request::SET_DEVICE_ID:
      // 广播修改设备ID (绿色部分)
      return {0x00, 0x06, 0x00, 0x0A,
              0x00, req->new_device_id,
              static_cast<uint8_t>(0x29), static_cast<uint8_t>(0xD8)}; // CRC固定值

    default:
      RCLCPP_WARN(this->get_logger(), "Unknown command type: %d", req->command_type);
      return {};
    }
  }

  void handleCommand(const Request& req, const Response& res)
  { 
    // RCLCPP_INFO(this->get_logger(), "Received command: %d", req->command_type);

    // 暂停状态查询
    pause_status_query_ = true;

    std::vector<uint8_t> frame = createCommandFrame(req);
    
    for (uint8_t byte : frame)
    {
      RCLCPP_DEBUG(this->get_logger(), "0x%02X ", byte);
    }

    if (!frame.empty())
    {
        try
        {
            {
              std::lock_guard<std::mutex> lock(serial_mutex_);
              serial_port_.Write(frame);
            }
            res->success = true;
            res->message = "Lift command sent successfully";
        }
        catch (...)
        {
            res->success = false;
            res->message = "Serial write failed";
        }
    }
    else
    {
        res->success = false;
        res->message = "Invalid command type";
    }
    // 延迟一段时间后恢复状态查询
    std::thread([this]() {
      std::this_thread::sleep_for(500ms);
      pause_status_query_ = false;
    }).detach();

  }

  // 高度转十六进制
  uint16_t height2Hex(uint16_t height_mm)
  {
    // 最小和最大高度限制（单位：毫米）
    const uint16_t MIN_HEIGHT_MM = 1;
    const uint16_t MAX_HEIGHT_MM = 380;

    // 确保输入在有效范围内
    uint16_t clamped_height = std::clamp(height_mm, MIN_HEIGHT_MM, MAX_HEIGHT_MM);

    // 返回与设备协议匹配的16位整数值
    return clamped_height;
  }

  // CRC校验计算
  uint16_t calculateCRC(const std::vector<uint8_t> &data)
  {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data)
    {
      crc ^= byte;
      for (int i = 0; i < 8; i++)
      {
        if (crc & 0x0001)
        {
          crc = (crc >> 1) ^ 0xA001;
        }
        else
        {
          crc >>= 1;
        }
      }
    }
    return crc;
  }

  // 查询回调
  void queryCallback(const LiftQuery::SharedPtr msg)
  {
    if (msg->query_position)
    {
      querySinglePosition(msg->motor_index);
    }

    if (msg->query_device_id)
    {
      querydeviceId();
    }
  }

  // 查询设备ID
  void querydeviceId()
  {
    std::vector<uint8_t> query = {0x00, 0x03, 0x00, 0x0A, 0x00, 0x02, 0xE5, 0xD8};
    try
    {
      serial_port_.Write(query);
    }
    catch (...)
    {
      RCLCPP_ERROR(this->get_logger(), "Failed to send device ID query");
    }
  }

  // 查询电机位置
  void querySinglePosition(uint8_t motor_index)
  {
    if (motor_index == 0)
    { // 查询所有电机
      for (int i = 1; i <= 4; i++)
      {
        queryPosition(i);
      }
      return;
    }

    if (motor_index < 1 || motor_index > 4)
    {
      RCLCPP_WARN(this->get_logger(), "Invalid motor index: %d", motor_index);
      return;
    }

    queryPosition(motor_index);
  }

  void queryPosition(int motor)
  {
    const std::vector<std::vector<uint8_t>> MOTOR_QUERIES = {
        {0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x25, 0xCA}, // 电机1
        {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A}, // 电机2
        {0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xCB}, // 电机3
        {0x01, 0x03, 0x00, 0x05, 0x00, 0x01, 0x94, 0x0B}  // 电机4
    };

    if (motor < 1 || motor > 4)
      return;

    try
    {
      std::lock_guard<std::mutex> lock(serial_mutex_);
      serial_port_.Write(MOTOR_QUERIES[motor - 1]);
    }
    catch (...)
    {
      RCLCPP_ERROR(this->get_logger(), "Failed to query motor %d position", motor);
    }
  }

  // 定时状态查询
  void statusTimerCallback()
  {
    // 如果正在处理其他命令，跳过本次查询
    if (pause_status_query_) {
      // RCLCPP_INFO(this->get_logger(), "Skipping status query");
      return;
    }

    auto query_msg = std::make_unique<LiftQuery>();
    query_msg->query_position = true; // 请求位置反馈
    query_msg->motor_index = 1; // 查询电机1
    query_pub_->publish(std::move(query_msg));
  }

  void printHex(const uint8_t* data, size_t length) {
      for (size_t i = 0; i < length; ++i) {
          RCLCPP_INFO(this->get_logger(), "0x%02X ", data[i]);
      }
  }

  // 串口数据读取线程
  void serialReadThread()
  {
    const size_t BUFFER_SIZE = 256;
    std::vector<uint8_t> read_buffer(BUFFER_SIZE, 0);
    // LibSerial::DataBuffer read_buffer(BUFFER_SIZE, 0);
    
    while (rclcpp::ok() && !terminate_thread_)
    {
      try
      {
        if (serial_port_.IsDataAvailable()) {
          size_t bytes_read = serial_port_.GetNumberOfBytesAvailable();
          serial_port_.Read(read_buffer, bytes_read, timeout_ms_); // 实际读取的字节数会更新
          RCLCPP_DEBUG(this->get_logger(), "Read %zu bytes", read_buffer.size());
          // if (bytes_read > 0) {
          //     printHex(read_buffer.data(), bytes_read);
          // }
        }
        std::this_thread::sleep_for(10ms); // 避免CPU占用过高

        if (!read_buffer.empty())
        {
          processSerialData(read_buffer.data(), read_buffer.size());
          
          read_buffer.clear(); // 清空缓冲区
        }
      }
      catch (const LibSerial::ReadTimeout &)
      {
        // 超时正常，继续下一次读取
      }
      catch (...)
      {
        RCLCPP_ERROR(this->get_logger(), "Serial read error");
        std::this_thread::sleep_for(100ms);
      }
    }
  }

  // 处理串口响应
  void processSerialData(const uint8_t *data, size_t length)
  {
    // 解析电机位置响应 (01 03 02 [高字节] [低字节])
    if (length >= 5 && data[1] == 0x03)
    {
      int motor_index = -1;

      // 根据寄存器地址确定电机
      uint16_t reg_address = data[2];
      switch (reg_address)
      {
      case 0x02:
        motor_index = 0;
        break; // 电机1
      case 0x03:
        motor_index = 1;
        break; // 电机2
      case 0x04:
        motor_index = 2;
        break; // 电机3
      case 0x05:
        motor_index = 3;
        break; // 电机4
      }

      if (motor_index >= 0 && length >= 7)
      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_height_mm = (data[3] << 8) | data[4];
      
        RCLCPP_DEBUG(this->get_logger(), "Motor %d: %d mm", motor_index, current_height_mm);
        // 更新状态
        if (current_status_.positions.size() < 4)
        {
          current_status_.positions.resize(4, 0.0);
        }
        current_status_.positions[motor_index] = current_height_mm;
        current_status_.device_id = device_id_;
      }
    }
    else if (length >= 7 && data[1] == 0x06)
    {
      
    }
    
    // 设备ID查询响应处理
    // ... (可扩展)

    // 发布状态更新
    publishCurrentStatus();
  }

  void publishCurrentStatus()
  {
    auto status_msg = std::make_unique<LiftStatus>();
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      *status_msg = current_status_;
    }
    status_pub_->publish(std::move(status_msg));
  }

private:
  // 成员变量
  LibSerial::SerialPort serial_port_;
  std::string port_name_;
  int timeout_ms_;
  uint8_t device_id_;
  uint16_t step_size_;
  std::mutex serial_mutex_;
  std::atomic<bool> pause_status_query_{false}; // 暂停状态查询标志


  rclcpp::Service<LiftCommand>::SharedPtr command_srv_;
  rclcpp::Subscription<LiftQuery>::SharedPtr query_sub_;
  rclcpp::Publisher<LiftStatus>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr status_timer_;
  rclcpp::Publisher<LiftQuery>::SharedPtr query_pub_;

  std::thread serial_thread_;
  std::atomic<bool> terminate_thread_{false};
  std::mutex status_mutex_;
  LiftStatus current_status_;
  uint16_t current_height_mm = 1;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<LiftController>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}