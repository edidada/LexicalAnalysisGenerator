cmake_minimum_required(VERSION 3.12)
project(LexicalAnalysisGenerator)

set(CMAKE_CXX_STANDARD 11)
# 启用 Qt 的自动处理
set(CMAKE_AUTOMOC ON)  # 自动运行 moc（处理 Q_OBJECT）
set(CMAKE_AUTORCC ON)  # 自动处理资源文件（.qrc，如果有）
set(CMAKE_AUTOUIC ON)  # 自动处理 .ui 文件（生成 ui_widget.h）
find_package(Qt5 COMPONENTS Widgets REQUIRED)
add_executable(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/code/compiler_lab_2/main.cpp
        ${CMAKE_SOURCE_DIR}/code/compiler_lab_2/widget.cpp
        ${CMAKE_SOURCE_DIR}/code/compiler_lab_2/widget.ui
)
# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Widgets
)