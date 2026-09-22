#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "common_interfaces/action/task_js.hpp"

using TaskJs = common_interfaces::action::TaskJs;
using GoalHandleTaskJs = rclcpp_action::ClientGoalHandle<TaskJs>;

class TaskClient : public rclcpp::Node {
public:
    TaskClient() : Node("task_client") {
        client_ = rclcpp_action::create_client<TaskJs>(this, "/execute_js_task");
    }

    void send_goal(const std::string &task_cfg) {
        if (!client_->wait_for_action_server(std::chrono::seconds(10))) {
            RCLCPP_ERROR(this->get_logger(), "Action server not available.");
            return;
        }

        auto goal_msg = TaskJs::Goal();
        goal_msg.task_cfg = task_cfg;

        RCLCPP_INFO(this->get_logger(), "Sending goal: %s", task_cfg.c_str());

        auto send_goal_options = rclcpp_action::Client<TaskJs>::SendGoalOptions();
        send_goal_options.goal_response_callback =
            std::bind(&TaskClient::goal_response_callback, this, std::placeholders::_1);
        send_goal_options.feedback_callback =
            std::bind(&TaskClient::feedback_callback, this, std::placeholders::_1, std::placeholders::_2);
        send_goal_options.result_callback =
            std::bind(&TaskClient::result_callback, this, std::placeholders::_1);

        client_->async_send_goal(goal_msg, send_goal_options);
    }

private:
    rclcpp_action::Client<TaskJs>::SharedPtr client_;

    void goal_response_callback(const GoalHandleTaskJs::SharedPtr &goal_handle) {
        if (!goal_handle) {
            RCLCPP_ERROR(this->get_logger(), "Goal was rejected by server.");
        } else {
            RCLCPP_INFO(this->get_logger(), "Goal accepted by server, waiting for result.");
        }
    }

    void feedback_callback(
        GoalHandleTaskJs::SharedPtr,
        const std::shared_ptr<const TaskJs::Feedback> feedback) {
        RCLCPP_INFO(this->get_logger(), "Progress: %d%%", feedback->progress_percent);
    }

    void result_callback(const GoalHandleTaskJs::WrappedResult &result) {
        switch (result.code) {
            case rclcpp_action::ResultCode::SUCCEEDED:
                RCLCPP_INFO(this->get_logger(), "Task completed with result code: %d", result.result->result_code);
                break;
            case rclcpp_action::ResultCode::ABORTED:
                RCLCPP_ERROR(this->get_logger(), "Task was aborted.");
                break;
            case rclcpp_action::ResultCode::CANCELED:
                RCLCPP_ERROR(this->get_logger(), "Task was canceled.");
                break;
            default:
                RCLCPP_ERROR(this->get_logger(), "Unknown result code.");
                break;
        }
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TaskClient>();

    if (argc < 2) {
        RCLCPP_ERROR(node->get_logger(), "Usage: task_client <task_cfg>");
        return 1;
    }

    node->send_goal(argv[1]);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}