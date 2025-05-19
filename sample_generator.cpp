#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstdint>

/**
 * @brief 生成简单的H264测试数据文件
 * 
 * 这个程序创建一个简单的二进制文件，模拟H264流数据
 * 用于测试MP4Writer库的功能
 */
int main(int argc, char* argv[]) {
    // 默认输出文件名
    std::string outputFile = "sample.h264";
    if (argc > 1) {
        outputFile = argv[1];
    }
    
    // 帧数和帧大小
    const int frameCount = 100;
    const int frameSize = 4096;
    
    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    // 打开输出文件
    std::ofstream file(outputFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法创建输出文件: " << outputFile << std::endl;
        return 1;
    }
    
    std::cout << "生成 " << frameCount << " 帧模拟H264数据，每帧 " << frameSize << " 字节" << std::endl;
    
    // 生成模拟H264数据
    std::vector<uint8_t> buffer(frameSize);
    
    // H264起始码
    const uint8_t startCode[] = {0x00, 0x00, 0x00, 0x01};
    
    for (int i = 0; i < frameCount; i++) {
        // 填充随机数据
        for (int j = 0; j < frameSize; j++) {
            buffer[j] = static_cast<uint8_t>(dis(gen));
        }
        
        // 在帧开始处添加H264起始码
        std::memcpy(buffer.data(), startCode, sizeof(startCode));
        
        // 每10帧设置一个关键帧标记（I帧）
        if (i % 10 == 0) {
            // NAL类型为5表示IDR帧（关键帧）
            buffer[4] = 0x65; // 0x65 = 0b01100101 (NAL类型5)
        } else {
            // NAL类型为1表示非IDR帧（P帧）
            buffer[4] = 0x41; // 0x41 = 0b01000001 (NAL类型1)
        }
        
        // 写入文件
        file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
    }
    
    file.close();
    std::cout << "模拟H264数据已生成到文件: " << outputFile << std::endl;
    
    return 0;
}