# MP4Writer 库

这是一个用于将H264流写入MP4文件的C++库，提供简单的接口进行视频录制，并根据开始和结束时间自动命名MP4文件。

## 功能特点

- 简单的开始/结束录制接口
- 基于开始和结束时间自动命名MP4文件
- 支持H264视频流写入
- 基于FFmpeg库实现，提供可靠的视频处理能力

## 依赖项

- C++17或更高版本
- FFmpeg库（libavformat, libavcodec, libavutil）
- CMake 3.10或更高版本（用于构建）

## 编译方法

确保已安装FFmpeg开发库和CMake，然后执行以下命令：

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译
cmake --build .
```

## 使用方法

### 基本用法

```cpp
#include "mp4writer.h"

// 创建MP4Writer实例，指定输出目录和文件前缀
MP4Writer writer("./output", "video");

// 开始录制，指定视频宽度、高度和帧率
writer.start(1280, 720, 25);

// 写入H264帧数据
// data: H264帧数据指针
// size: 数据大小
// isKeyFrame: 是否为关键帧
// pts: 显示时间戳（毫秒）
writer.writeFrame(data, size, isKeyFrame, pts);

// 停止录制，返回生成的文件路径
std::string filePath = writer.stop();
```

### 文件命名格式

生成的MP4文件将按照以下格式命名：

```
[前缀]_YYYYMMDD_HHMMSS_mmm_to_YYYYMMDD_HHMMSS_mmm.mp4
```

其中：
- `[前缀]`：创建MP4Writer时指定的文件前缀
- `YYYYMMDD_HHMMSS_mmm`：年月日_时分秒_毫秒格式的时间戳

## 示例代码

详细的使用示例请参考 `main.cpp` 文件，其中演示了如何：

1. 创建MP4Writer实例
2. 开始录制
3. 写入H264帧数据
4. 停止录制并获取生成的文件路径

## 注意事项

- 确保输出目录存在或有权限创建
- 写入的H264数据必须是有效的H264编码流
- 如果在录制过程中发生错误，可以检查返回值和错误输出

## 许可证

本项目采用MIT许可证。详情请参阅LICENSE文件。