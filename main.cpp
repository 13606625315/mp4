#include "mp4writer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

// 模拟H264帧数据的简单结构
struct H264Frame {
    std::vector<uint8_t> data;
    bool isKeyFrame;
    int64_t pts; // 毫秒
};

// 模拟从文件读取H264数据（实际应用中可能从网络或摄像头获取）
std::vector<H264Frame> loadSampleH264Frames(const std::string& filename) {
    // 这里只是示例，实际应用中需要正确解析H264文件
    std::vector<H264Frame> frames;
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "无法打开H264文件: " << filename << std::endl;
        return frames;
    }
    
    // 简化示例：每4KB作为一帧，每10帧一个关键帧
    const int frameSize = 4096;
    std::vector<uint8_t> buffer(frameSize);
    int frameCount = 0;
    int64_t currentPts = 0;
    
    while (file.read(reinterpret_cast<char*>(buffer.data()), frameSize)) {
        H264Frame frame;
        frame.data = buffer;
        frame.isKeyFrame = (frameCount % 10 == 0); // 每10帧一个关键帧
        frame.pts = currentPts;
        
        frames.push_back(frame);
        frameCount++;
        currentPts += 40; // 假设25fps，帧间隔40ms
    }
    
    std::cout << "加载了 " << frames.size() << " 帧H264数据" << std::endl;
    return frames;
}

// 演示如何使用MP4Writer
void demoMP4Writer() {
    // 创建MP4Writer实例
    std::string outputDir = "./output";
    std::string filePrefix = "video";
    MP4Writer writer(outputDir, filePrefix);
    
    // 假设我们有一些H264帧数据
    // 实际应用中，这些数据可能来自网络流、摄像头等
    std::vector<H264Frame> frames = loadSampleH264Frames("sample.h264");
    
    if (frames.empty()) {
        std::cout << "没有H264帧数据，使用模拟数据进行演示" << std::endl;
        
        // 创建一些模拟帧数据用于演示
        for (int i = 0; i < 100; i++) {
            H264Frame frame;
            frame.data.resize(1024, static_cast<uint8_t>(i % 256)); // 简单填充一些数据
            frame.isKeyFrame = (i % 10 == 0); // 每10帧一个关键帧
            frame.pts = i * 40; // 假设25fps，帧间隔40ms
            frames.push_back(frame);
        }
    }
    
    // 开始录制
    if (!writer.start(1280, 720, 25)) {
        std::cerr << "开始录制失败" << std::endl;
        return;
    }
    
    std::cout << "开始录制MP4文件..." << std::endl;
    
    // 写入帧数据
    for (const auto& frame : frames) {
        if (!writer.writeFrame(frame.data.data(), frame.data.size(), 
                              frame.isKeyFrame, frame.pts)) {
            std::cerr << "写入帧失败" << std::endl;
            break;
        }
        
        // 模拟实时处理的延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 停止录制
    std::string filePath = writer.stop();
    std::cout << "MP4文件录制完成: " << filePath << std::endl;
}

int main() {
    std::cout << "H264流写入MP4演示程序" << std::endl;
    
    try {
        demoMP4Writer();
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "演示完成" << std::endl;
    return 0;
}