cmake_minimum_required(VERSION 3.8)
# Jammy defaults to g++-11, which has no <expected>. Only takes effect on a clean configure.
set(CMAKE_CXX_COMPILER g++-12)
project(quorum_monitors)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                      -Wnon-virtual-dtor -Werror)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(eigen3_cmake_module REQUIRED)
find_package(Eigen3 REQUIRED)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  # formatting is governed by .clang-format, not by uncrustify
  set(ament_cmake_uncrustify_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_gnss_monitor test/test_gnss_monitor.cpp)
  target_include_directories(test_gnss_monitor PRIVATE include)
  # Eigen arrives as an imported target, which keeps its headers out of -Wconversion
  target_link_libraries(test_gnss_monitor Eigen3::Eigen)

  ament_add_gtest(test_velocity_reference test/test_velocity_reference.cpp)
  target_include_directories(test_velocity_reference PRIVATE include)
  target_link_libraries(test_velocity_reference Eigen3::Eigen)
endif()

add_executable(gnss_monitor_node src/gnss_monitor_node.cpp)
target_include_directories(gnss_monitor_node PRIVATE include)
target_link_libraries(gnss_monitor_node Eigen3::Eigen)
ament_target_dependencies(gnss_monitor_node rclcpp px4_msgs geometry_msgs)

install(TARGETS gnss_monitor_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()
