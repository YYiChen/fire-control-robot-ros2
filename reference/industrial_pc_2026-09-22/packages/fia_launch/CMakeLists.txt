cmake_minimum_required(VERSION 3.8)
project(fia_launch)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

ament_package()


install(DIRECTORY launch DESTINATION share/${PROJECT_NAME}
  PATTERN "setup_assistant.launch" EXCLUDE) # 排除
install(DIRECTORY configs DESTINATION share/${PROJECT_NAME})
install(DIRECTORY script DESTINATION share/${PROJECT_NAME})
install(DIRECTORY certs DESTINATION share/${PROJECT_NAME})
