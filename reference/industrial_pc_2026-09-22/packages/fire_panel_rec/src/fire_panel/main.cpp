#include "fire_panel_rec.hpp"
#include <gflags/gflags.h>

int main(int argc, char *argv[])
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FirePanelRec>(argc, argv));
  rclcpp::shutdown();
  return 0;
}
