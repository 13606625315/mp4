#include "mp4writer.h"

// 包含FFmpeg头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

#include <iostream>
#include <filesystem>

// 构造函数
MP4Writer::MP4Writer(const std::string& outputDir, const std::string& prefix)
    : m_outputDir(outputDir)
    , m_filePrefix(prefix)
    , m_isRecording(false)
    , m_formatContext(nullptr)
    , m_videoStream(nullptr)
    , m_codecContext(nullptr)
    , m_nextPts(0)
{
    // 确保输出目录存在
    std::filesystem::path dirPath(outputDir);
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    // 初始化FFmpeg（仅在第一次使用时需要）
    static bool ffmpegInitialized = false;
    if (!ffmpegInitialized) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
#endif
        ffmpegInitialized = true;
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
    
    // 初始化FFmpeg
    if (!initializeFFmpeg(width, height, frameRate)) {
        std::cerr << "Failed to initialize FFmpeg" << std::endl;
        return false;
    }

    // 创建临时文件路径（最终文件名将在stop时确定）
    m_currentFilePath = m_outputDir + "/" + m_filePrefix + "_temp.mp4";

    // 打开输出文件
    if (avio_open(&m_formatContext->pb, m_currentFilePath.c_str(), AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Could not open output file: " << m_currentFilePath << std::endl;
        cleanupFFmpeg();
        return false;
    }

    // 写入文件头
    if (avformat_write_header(m_formatContext, nullptr) < 0) {
        std::cerr << "Error writing header" << std::endl;
        cleanupFFmpeg();
        return false;
    }

    m_isRecording = true;
    m_nextPts = 0;
    return true;
}

// 写入H264帧
bool MP4Writer::writeFrame(const uint8_t* data, int size, bool keyFrame, int64_t pts)
{
    if (!m_isRecording || !m_formatContext || !m_videoStream) {
        return false;
    }

    AVPacket pkt = { 0 };
    av_init_packet(&pkt);

    pkt.data = const_cast<uint8_t*>(data);
    pkt.size = size;
    
    // 如果提供了pts，使用它，否则使用自增的pts
    if (pts > 0) {
        // 转换为FFmpeg时间基准
        pkt.pts = av_rescale_q(pts, AVRational{1, 1000}, m_videoStream->time_base);
    } else {
        pkt.pts = m_nextPts++;
    }
    
    pkt.dts = pkt.pts;
    pkt.stream_index = m_videoStream->index;
    
    if (keyFrame) {
        pkt.flags |= AV_PKT_FLAG_KEY;
    }

    // 写入帧
    int ret = av_interleaved_write_frame(m_formatContext, &pkt);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Error writing frame: " << errBuf << std::endl;
        return false;
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

    // 写入文件尾
    if (m_formatContext) {
        av_write_trailer(m_formatContext);
        if (m_formatContext->pb) {
            avio_close(m_formatContext->pb);
        }
    }

    // 清理FFmpeg资源
    cleanupFFmpeg();

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

// 初始化FFmpeg
bool MP4Writer::initializeFFmpeg(int width, int height, int frameRate)
{
    // 创建输出格式上下文
    int ret = avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, m_currentFilePath.c_str());
    if (ret < 0 || !m_formatContext) {
        std::cerr << "Could not create output context" << std::endl;
        return false;
    }

    // 查找H264编码器
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "H264 codec not found" << std::endl;
        cleanupFFmpeg();
        return false;
    }

    // 创建视频流
    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) {
        std::cerr << "Could not create video stream" << std::endl;
        cleanupFFmpeg();
        return false;
    }

    // 设置视频流参数
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        std::cerr << "Could not allocate codec context" << std::endl;
        cleanupFFmpeg();
        return false;
    }

    m_codecContext->codec_id = AV_CODEC_ID_H264;
    m_codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    m_codecContext->width = width;
    m_codecContext->height = height;
    m_codecContext->time_base = AVRational{1, frameRate};
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

    // 复制编解码器参数到流
    ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_codecContext);
    if (ret < 0) {
        std::cerr << "Could not copy codec parameters to stream" << std::endl;
        cleanupFFmpeg();
        return false;
    }

    // 设置流时间基准
    m_videoStream->time_base = m_codecContext->time_base;

    // 如果格式需要全局头信息
    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    return true;
}

// 清理FFmpeg资源
void MP4Writer::cleanupFFmpeg()
{
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }

    if (m_formatContext) {
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE) && m_formatContext->pb) {
            avio_close(m_formatContext->pb);
        }
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }

    m_videoStream = nullptr; // 流会随着格式上下文一起释放
}