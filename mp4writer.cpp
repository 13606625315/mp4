#include "mp4writer.h"

#include <iostream>
#include <filesystem>
#include <cstring>
#include <vector>

// 构造函数
MP4Writer::MP4Writer(const std::string& outputDir, const std::string& prefix)
    : m_outputDir(outputDir)
    , m_filePrefix(prefix)
    , m_isRecording(false)
    , m_mp4File(MP4_INVALID_FILE_HANDLE)
    , m_videoTrack(MP4_INVALID_TRACK_ID)
    , m_timeScale(0)
    , m_frameRate(0)
    , m_nextPts(0)
{
    // 确保输出目录存在
    std::filesystem::path dirPath(outputDir);
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }
}

// 析构函数
MP4Writer::~MP4Writer()
{
    // 确保停止录制
    if (m_isRecording) {
        stop();
    }
}

// 开始录制
bool MP4Writer::start(int width, int height, int frameRate)
{
    // 如果已经在录制，先停止
    if (m_isRecording) {
        stop();
    }

    // 记录开始时间
    m_startTime = std::chrono::system_clock::now();
    
    // 初始化MP4
    if (!initializeMP4(width, height, frameRate)) {
        std::cerr << "Failed to initialize MP4" << std::endl;
        return false;
    }

    // 创建临时文件路径（最终文件名将在stop时确定）
    m_currentFilePath = m_outputDir + "/" + m_filePrefix + "_temp.mp4";

    // 创建MP4文件
    m_mp4File = MP4Create(m_currentFilePath.c_str(), 0);
    if (m_mp4File == MP4_INVALID_FILE_HANDLE) {
        std::cerr << "Could not create output file: " << m_currentFilePath << std::endl;
        return false;
    }

    // 设置MP4文件属性
    MP4SetTimeScale(m_mp4File, m_timeScale);

    // 添加H264视频轨道
    m_videoTrack = MP4AddH264VideoTrack(
        m_mp4File,
        m_timeScale,
        m_timeScale / m_frameRate,
        width,
        height,
        0x64, // AVCProfileIndication - Baseline profile
        0x00, // profile_compat
        0x1F, // AVCLevelIndication - Level 3.1
        3     // 4 bytes length before NAL units
    );

    if (m_videoTrack == MP4_INVALID_TRACK_ID) {
        std::cerr << "Could not add video track" << std::endl;
        cleanupMP4();
        return false;
    }

    // 设置视频属性
    MP4SetVideoProfileLevel(m_mp4File, 0x7F); // Simple profile

    // 添加默认的SPS和PPS
    // 这是一个基本的SPS示例，实际应用中应该从编码器获取
    uint8_t sps[] = {
        0x67, 0x64, 0x00, 0x1F, 0xAC, 0xD9, 0x40, 0x50,
        0x05, 0xBB, 0x01, 0x10, 0x00, 0x00, 0x03, 0x00,
        0x10, 0x00, 0x00, 0x03, 0x03, 0xC0, 0xF1, 0x42,
        0x99, 0x60
    };
    
    // 这是一个基本的PPS示例
    uint8_t pps[] = {
        0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0
    };

    // 添加SPS和PPS到MP4文件
    if (!MP4AddH264SequenceParameterSet(m_mp4File, m_videoTrack, sps, sizeof(sps))) {
        std::cerr << "Could not add SPS" << std::endl;
        cleanupMP4();
        return false;
    }

    if (!MP4AddH264PictureParameterSet(m_mp4File, m_videoTrack, pps, sizeof(pps))) {
        std::cerr << "Could not add PPS" << std::endl;
        cleanupMP4();
        return false;
    }

    m_isRecording = true;
    m_nextPts = 0;
    return true;
}

// 写入H264帧
bool MP4Writer::writeFrame(const uint8_t* data, int size, bool keyFrame, int64_t pts)
{
    if (!m_isRecording || m_mp4File == MP4_INVALID_FILE_HANDLE || m_videoTrack == MP4_INVALID_TRACK_ID) {
        return false;
    }

    // 缓冲区用于转换H264数据格式
    std::vector<uint8_t> buffer(size + 10); // 额外空间用于可能的格式转换
    
    // 转换H264数据格式（从NALU开始码到长度前缀）
    int convertedSize = convertH264Data(data, size, buffer.data());
    if (convertedSize <= 0) {
        // 如果是SPS或PPS帧，convertH264Data会返回0，这是正常的，不需要报错
        return true;
    }
    
    // 如果提供了pts，使用它，否则使用自增的pts
    MP4Duration sampleDuration = MP4_INVALID_DURATION;
    if (pts > 0) {
        // 转换为MP4时间基准
        m_nextPts = pts * m_timeScale / 1000;
    }
    
    // 写入帧
    if (!MP4WriteSample(m_mp4File, m_videoTrack, buffer.data(), convertedSize, sampleDuration, 0, keyFrame)) {
        std::cerr << "Error writing frame" << std::endl;
        return false;
    }

    // 如果没有提供pts，则递增
    if (pts <= 0) {
        m_nextPts += m_timeScale / m_frameRate;
    }

    return true;
}

// 停止录制
std::string MP4Writer::stop()
{
    if (!m_isRecording) {
        return "";
    }

    // 记录结束时间
    m_endTime = std::chrono::system_clock::now();

    // 关闭MP4文件
    if (m_mp4File != MP4_INVALID_FILE_HANDLE) {
        MP4Close(m_mp4File, 0);
    }

    // 清理MP4资源
    cleanupMP4();

    // 生成基于时间的最终文件名
    std::string finalFilePath = m_outputDir + "/" + generateFileName(m_startTime, m_endTime);
    
    // 重命名文件
    try {
        std::filesystem::rename(m_currentFilePath, finalFilePath);
        m_currentFilePath = finalFilePath;
    } catch (const std::exception& e) {
        std::cerr << "Error renaming file: " << e.what() << std::endl;
        // 如果重命名失败，保留原文件名
    }

    m_isRecording = false;
    return m_currentFilePath;
}

// 生成基于时间的文件名
std::string MP4Writer::generateFileName(const std::chrono::system_clock::time_point& startTime, 
                                      const std::chrono::system_clock::time_point& endTime)
{
    std::string fileName = m_filePrefix;
    if (!fileName.empty()) {
        fileName += "_";
    }

    // 添加开始时间
    fileName += formatTime(startTime);

    // 如果有结束时间，也添加
    if (endTime != std::chrono::system_clock::time_point{}) {
        fileName += "_to_" + formatTime(endTime);
    }

    fileName += ".mp4";
    return fileName;
}

// 格式化时间为字符串
std::string MP4Writer::formatTime(const std::chrono::system_clock::time_point& timePoint)
{
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timePoint.time_since_epoch() % std::chrono::seconds(1));

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 初始化MP4
bool MP4Writer::initializeMP4(int width, int height, int frameRate)
{
    // 设置时间刻度和帧率
    m_timeScale = 90000;  // 常用的MP4时间刻度
    m_frameRate = frameRate;
    
    return true;
}

// 清理MP4资源
void MP4Writer::cleanupMP4()
{
    if (m_mp4File != MP4_INVALID_FILE_HANDLE) {
        MP4Close(m_mp4File, 0);
        m_mp4File = MP4_INVALID_FILE_HANDLE;
    }
    
    m_videoTrack = MP4_INVALID_TRACK_ID;
}

// 转换H264数据格式
int MP4Writer::convertH264Data(const uint8_t* data, int size, uint8_t* outData)
{
    // 检查数据是否以H.264开始码开头 (0x00 0x00 0x00 0x01 或 0x00 0x00 0x01)
    if (size < 4) {
        return 0;
    }
    
    int startCodeSize = 0;
    if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        startCodeSize = 4;  // 4字节开始码
    } else if (data[0] == 0 && data[1] == 0 && data[2] == 1) {
        startCodeSize = 3;  // 3字节开始码
    }
    
    if (startCodeSize > 0) {
        // 获取NALU类型 (5位，在第一个字节的低5位)
        uint8_t naluType = data[startCodeSize] & 0x1F;
        
        // 检查是否为SPS或PPS，如果是，我们应该跳过它们，因为它们已经在start()中添加
        // SPS = 7, PPS = 8
        if (naluType == 7 || naluType == 8) {
            std::cout << "跳过" << (naluType == 7 ? "SPS" : "PPS") << "NALU" << std::endl;
            return 0;
        }
        
        // 替换开始码为长度前缀
        uint32_t naluSize = size - startCodeSize;
        outData[0] = (naluSize >> 24) & 0xFF;
        outData[1] = (naluSize >> 16) & 0xFF;
        outData[2] = (naluSize >> 8) & 0xFF;
        outData[3] = naluSize & 0xFF;
        
        // 复制NALU数据
        memcpy(outData + 4, data + startCodeSize, naluSize);
        
        return naluSize + 4;
    } else {
        // 检查是否已经是MP4格式（带有长度前缀）
        uint32_t naluSize = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        
        // 验证长度是否合理
        if (naluSize + 4 <= size) {
            // 获取NALU类型
            uint8_t naluType = data[4] & 0x1F;
            
            // 检查是否为SPS或PPS
            if (naluType == 7 || naluType == 8) {
                std::cout << "跳过" << (naluType == 7 ? "SPS" : "PPS") << "NALU" << std::endl;
                return 0;
            }
            
            // 如果数据已经是MP4格式（带有长度前缀），直接复制
            memcpy(outData, data, size);
            return size;
        } else {
            // 长度不合理，可能不是MP4格式
            std::cerr << "无效的H264数据格式" << std::endl;
            return 0;
        }
    }
}