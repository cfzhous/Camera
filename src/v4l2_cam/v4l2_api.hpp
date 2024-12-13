#pragma once

// std include
#include <functional>
#include <iostream>
#include <vector>
#include <map>

// linux include
#include <linux/videodev2.h>
#include "utils/ret.hpp"

namespace v4l2_api {

struct PlaneStart {
    void *start;
};

struct VideoBuffer
{
    void *buffer;
    size_t length;
    PlaneStart *pstart;
};

struct ImageInfo
{
    uint32_t pixfmt;
    uint32_t width;
    uint32_t height;
};

class V4l2Cam
{

    using DataCallback = std::function<void(const uint8_t *data, size_t len)>;

public:
    V4l2Cam();
    ~V4l2Cam();

    awe::Ret<bool> open(const std::string &name, int w = 0, int h = 0, uint32_t pixfmt = 0);
    awe::Ret<bool> close();
    awe::Ret<bool> capture(const DataCallback &dataCb);
    awe::Ret<bool> capture(uint8_t *data, size_t len);
    size_t bufferSize();
    awe::Ret<bool> initDevice(void *usrPtr = nullptr);
    
    operator bool();

private:
    std::string __m_devName;
    int __m_width;
    int __m_height;
    uint32_t __m_pixfmt = V4L2_PIX_FMT_YUYV;
    int __m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int __m_memoryType = V4L2_MEMORY_USERPTR;
    const int __m_numOfBuffers = 4;
    int __m_camFd;
    std::vector< VideoBuffer > __m_buffers;
    int __m_reqCnt = 0;
    size_t __m_offset[16] = { 0 };
    struct v4l2_format __m_v4l2Fmt;
    struct v4l2_plane *__m_tmpPlane;

    std::map<std::string, std::vector<ImageInfo>> __m_supportList;
    void __initMmap(void);
    void __initUsrPtr(unsigned int bufferSize, void *usrPtr); 
    bool __prepareBuffer();
    bool __prepareBufferMplane();

    bool __freeBufferMplane();
};

} // namespace v4l2_api 