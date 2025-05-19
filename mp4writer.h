#ifndef MP4WRITER_H
#define MP4WRITER_H

#include <string>
#include <ctime>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

// 前向声明，避免包含完整的FFmpeg头文件
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVPacket;

/**
 * @brief H264流写入MP4的类
 * 
 * 提供简单的接口将H264流写入MP4文件，文件名按照开始和结束时间自动命名
 */
class MP4Writer {
public:
    /**
     * @brief 构造函数
     * @param outputDir 输出目录路径
     * @param prefix 文件名前缀（可选）
     */
    MP4Writer(const std::string& outputDir, const std::string& prefix = "");

    /**
     * @brief 析构函数
     */
    ~MP4Writer();

    /**
     * @brief 开始录制
     * @param width 视频宽度
     * @param height 视频高度
     * @param frameRate 帧率
     * @return 是否成功开始录制
     */
    bool start(int width, int height, int frameRate = 25);

    /**
     * @brief 写入H264帧数据
     * @param data H264帧数据
     * @param size 数据大小
     * @param keyFrame 是否为关键帧
     * @param pts 显示时间戳（毫秒）
     * @return 是否成功写入
     */
    bool writeFrame(const uint8_t* data, int size, bool keyFrame, int64_t pts);

    /**
     * @brief 结束录制
     * @return 录制的文件路径
     */
    std::string stop();

    /**
     * @brief 检查是否正在录制
     * @return 是否正在录制
     */
    bool isRecording() const { return m_isRecording; }

    /**
     * @brief 获取当前录制的文件路径
     * @return 当前录制的文件路径
     */
    std::string getCurrentFilePath() const { return m_currentFilePath; }

private:
    /**
     * @brief 生成基于时间的文件名
     * @param startTime 开始时间
     * @param endTime 结束时间（可选）
     * @return 生成的文件名
     */
    std::string generateFileName(const std::chrono::system_clock::time_point& startTime, 
                                const std::chrono::system_clock::time_point& endTime = {});

    /**
     * @brief 格式化时间为字符串
     * @param timePoint 时间点
     * @return 格式化的时间字符串
     */
    std::string formatTime(const std::chrono::system_clock::time_point& timePoint);

    /**
     * @brief 初始化FFmpeg上下文
     * @param width 视频宽度
     * @param height 视频高度
     * @param frameRate 帧率
     * @return 是否成功初始化
     */
    bool initializeFFmpeg(int width, int height, int frameRate);

    /**
     * @brief 清理FFmpeg资源
     */
    void cleanupFFmpeg();

private:
    std::string m_outputDir;        // 输出目录
    std::string m_filePrefix;       // 文件名前缀
    std::string m_currentFilePath;  // 当前录制的文件路径
    bool m_isRecording;             // 是否正在录制

    // 记录开始和结束时间
    std::chrono::system_clock::time_point m_startTime;
    std::chrono::system_clock::time_point m_endTime;

    // FFmpeg相关成员
    AVFormatContext* m_formatContext;
    AVStream* m_videoStream;
    AVCodecContext* m_codecContext;
    int64_t m_nextPts;              // 下一帧的PTS
};

#endif // MP4WRITER_H