#include <deque>
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"


class PoseFilter {
private:
    std::deque<tf2::Transform> pose_history;
    int max_history_size;
    tf2::Vector3 filtered_position;
    tf2::Quaternion filtered_orientation;
    bool initialized;

public:
    PoseFilter(int history_size = 10) : max_history_size(history_size), initialized(false) {}
    
    tf2::Transform filterPose(const tf2::Transform& new_pose) {
        pose_history.push_back(new_pose);
        if (pose_history.size() > max_history_size) {
            pose_history.pop_front();
        }
        
        if (!initialized && pose_history.size() >= 3) {
            // 初始化滤波器
            filtered_position = new_pose.getOrigin();
            filtered_orientation = new_pose.getRotation();
            initialized = true;
        }
        
        if (initialized) {
            // 简单移动平均滤波
            tf2::Vector3 avg_position(0, 0, 0);
            for (const auto& pose : pose_history) {
                avg_position += pose.getOrigin();
            }
            avg_position /= pose_history.size();
            
            // 指数移动平均
            double alpha = 0.3; // 滤波系数
            filtered_position = alpha * new_pose.getOrigin() + (1 - alpha) * filtered_position;
            
            // 四元数滤波
            filtered_orientation = filtered_orientation.slerp(new_pose.getRotation(), alpha);
            filtered_orientation.normalize();
        }
        
        tf2::Transform filtered_pose;
        filtered_pose.setOrigin(filtered_position);
        filtered_pose.setRotation(filtered_orientation);
        return filtered_pose;
    }
};