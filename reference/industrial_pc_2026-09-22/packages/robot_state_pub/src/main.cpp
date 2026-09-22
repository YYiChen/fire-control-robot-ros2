#include <robot_status_publisher.hpp>


int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RobotStatusPublisher>());
  rclcpp::shutdown();
  return 0;
}