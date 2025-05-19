cmake_minimum_required(VERSION 3.10)
project(MP4Writer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找FFmpeg包
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
)

# 添加可执行文件
add_executable(mp4writer_demo
    main.cpp
    mp4writer.cpp
)

# 包含目录
target_include_directories(mp4writer_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 链接FFmpeg库
target_link_libraries(mp4writer_demo PRIVATE PkgConfig::FFMPEG)

# 如果需要创建库
add_library(mp4writer STATIC
    mp4writer.cpp
)

# 设置库的包含目录和链接库
target_include_directories(mp4writer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mp4writer PRIVATE PkgConfig::FFMPEG)

# 安装规则
install(TARGETS mp4writer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES mp4writer.h DESTINATION include)