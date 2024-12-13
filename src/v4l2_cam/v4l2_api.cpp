#include "v4l2_cam.hpp"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <sstream>

// 内部使用函数
static int __open(const char *dev, int flag)
{
    return open(dev, flag);
}

// 内部使用函数
static int __close(int fd)
{
    return close(fd);
}

namespace v4l2_api {

    V4l2Cam::V4l2Cam() : __m_devName("")
      , __m_width(0)
      , __m_height(0)
      , __m_camFd(0)
    {
    }

    V4l2Cam::~V4l2Cam()
    {
    }

    V4l2Cam::operator bool()
    {
        return __m_camFd != 0;
    }

    size_t V4l2Cam::bufferSize()
    {
        size_t bufSize =0;
        if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE) { bufSize = __m_width*__m_height*2*__m_numOfBuffers;}
        else if(__m_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) 
        {
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            bufSize = __m_width*__m_height*2*__m_numOfBuffers*numOfPlanes;    
        }
        return bufSize;
    }

    awe::Ret<bool> V4l2Cam::initDevice(void *usrPtr)
    {
        //获取v4l2_memory_type 类型
        if (usrPtr != nullptr)
        {
            __m_memoryType = V4L2_MEMORY_USERPTR;
            unsigned int min = __m_v4l2Fmt.fmt.pix.width * 2;
            if (__m_v4l2Fmt.fmt.pix.bytesperline < min)   __m_v4l2Fmt.fmt.pix.bytesperline = min;
            min = __m_v4l2Fmt.fmt.pix.bytesperline * __m_v4l2Fmt.fmt.pix.height;
            if (__m_v4l2Fmt.fmt.pix.sizeimage < min)   __m_v4l2Fmt.fmt.pix.sizeimage = min;
            __initUsrPtr(__m_v4l2Fmt.fmt.pix.sizeimage, usrPtr);
            std::cout << "use __initUsrPtr ......"<< std::endl;

        }
        else
        {
            __m_memoryType = V4L2_MEMORY_MMAP;
            __initMmap();
            std::cout << "use __initMmap ......"<< std::endl;
        }
        // buffer
        if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            __prepareBuffer();
            __m_tmpPlane = nullptr;
        }
        else if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            __prepareBufferMplane();
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            __m_tmpPlane = static_cast<struct v4l2_plane *>(calloc(numOfPlanes, sizeof(*__m_tmpPlane)));
        }
        // 打开设备视频流
        if (ioctl(__m_camFd, VIDIOC_STREAMON, &__m_type) < 0)
        {
            return {false, "video stream on failed"};
        }
        return true;
    }

    awe::Ret<bool> V4l2Cam::open(const std::string &name, int w, int h, uint32_t pixfmt)
    {
        if(operator bool())
        {
            return {false, "camera already opened"};
        }

        __m_devName = name;
        if(__m_devName.empty())
        {
            return {false, "camera name invalid"};
        }

        // 打开相机
        __m_camFd = __open(__m_devName.c_str(), O_RDWR);
        if (!__m_camFd)
        {
            return {false, "camera open failed"};
        }

        __m_width = w;
        __m_height = h;
        __m_pixfmt = pixfmt;
        /* 获取设备支持的操作 */
        struct v4l2_capability cap;
        memset(&cap, 0, sizeof(cap));
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        if (ioctl(__m_camFd, VIDIOC_QUERYCAP, &cap) < 0)
        {
            if (EINVAL == errno)
            { /*EINVAL为返回的错误值*/
                return {false, "not a v4l2 device"};
            }
            else
            {
                return {false, "unknown querycap error"};
            }
        }

        // 获取成功，检查是否有视频捕获功能
        if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        {
            __m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
        else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        {
            __m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        }
        else
        {
            return {false, "not a video capture device"};            
        }
        /* streaming I/O ioctls */
        if (!(cap.capabilities & V4L2_CAP_STREAMING))
        {
            return {false, "does not support streaming I/O"};
        }

        // 初始化input
        struct v4l2_input input;
        int ret = ioctl(__m_camFd, VIDIOC_G_INPUT, &input);

        // 检查视频标准
        v4l2_std_id std;
        do
        {
            ret = ioctl(__m_camFd, VIDIOC_QUERYSTD, &std);
            
        } while (ret == -1 && errno == EAGAIN);
        switch (std)
        {
        case V4L2_STD_NTSC:
            std::cout << "std: V4L2_STD_NTSC" << std::endl;
            break;
        case V4L2_STD_PAL:
            std::cout << "std: V4L2_STD_PAL" << std::endl;
            break;
        }
        memset(&cropcap, 0, sizeof(cropcap));
        cropcap.type = __m_type;

        if (0 == ioctl(__m_camFd, VIDIOC_CROPCAP, &cropcap))
        {
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c = cropcap.defrect; /* reset to default */
            if (-1 == ioctl(__m_camFd, VIDIOC_S_CROP, &crop))
            {
                switch (errno)
                {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
                }
            }
        }
        else
        {
            /* Errors ignored. */
        }

        // 获取支持的格式列表
        struct v4l2_fmtdesc fmtdesc;
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.type = __m_type;
        __m_supportList.clear();
        for(int i = 0; ; i++)
        {
            fmtdesc.index = i;
            int ret = ioctl(__m_camFd, VIDIOC_ENUM_FMT, &fmtdesc);
            if(-1 == ret)
            {
                break;
            }
        
            for(int j = 0; ; j++)
            {
                struct v4l2_frmsizeenum frmsize;
                struct v4l2_frmsizeenum frminsize;
                frminsize.pixel_format = fmtdesc.pixelformat;
                frmsize.pixel_format = fmtdesc.pixelformat;
                frmsize.index = j;
                frminsize.index = j;
                ret = ioctl(__m_camFd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
                if(-1 == ret)
                {
                    break;
                }

                ret = ioctl(__m_camFd, VIDIOC_ENUM_FRAMEINTERVALS, &frminsize);
                if(-1 == ret)
                {
                    break;
                }

                if(__m_width == 0 || __m_height == 0)
                {
                    __m_pixfmt = fmtdesc.pixelformat;
                    __m_width = frmsize.discrete.width;
                    __m_height = frmsize.discrete.height;
                }

                ImageInfo imgInfo;
                imgInfo.width = frmsize.discrete.width;
                imgInfo.height = frmsize.discrete.height;
                imgInfo.pixfmt = fmtdesc.pixelformat;

                __m_supportList[std::string(reinterpret_cast<char*>(&fmtdesc.description[0]))].emplace_back(imgInfo);
            }
        }
        memset(&__m_v4l2Fmt, 0, sizeof(__m_v4l2Fmt));
        __m_v4l2Fmt.type = __m_type;
        __m_v4l2Fmt.fmt.pix.width = __m_width;
        __m_v4l2Fmt.fmt.pix.height = __m_height;
        __m_v4l2Fmt.fmt.pix.pixelformat = __m_pixfmt;
        __m_v4l2Fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
        /* 设置设备捕获视频的格式 */
        if (ioctl(__m_camFd, VIDIOC_S_FMT, &__m_v4l2Fmt) < 0)
        {
            return {false, "iformat not supported"};
        }
        return true;
    }

    awe::Ret<bool> V4l2Cam::close()
    {
        std::stringstream ss;
        // 关闭流
        if(__m_camFd != -1)
        {
            int ret = ioctl(__m_camFd, VIDIOC_STREAMOFF, &__m_type);
            if(ret < 0)
            {
                ss << "stream off failed" << std::endl;
            }
        }

        if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            
            for (int j = 0; j < __m_reqCnt; j++)
            {
                munmap(__m_buffers[j].buffer, __m_buffers[j].length);
            }            
        }
        else if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            if(!__freeBufferMplane())
            {
                ss << "free buffer plane failed" << std::endl;
            }
        }

        if(__m_tmpPlane)
        {
            free(__m_tmpPlane);
            __m_tmpPlane = nullptr;
        }

        if(__m_camFd != -1)
        {
            __close(__m_camFd);
            __m_camFd = 0;
        }

        return {true, ss.str()};
    }

    awe::Ret<bool> V4l2Cam::capture(const DataCallback &dataCb)
    {
        struct v4l2_buffer captureBuf;
        memset(&captureBuf, 0, sizeof(captureBuf));
        captureBuf.type = __m_type;
        captureBuf.memory = __m_memoryType;

        if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            captureBuf.m.planes = __m_tmpPlane;
            captureBuf.length = numOfPlanes;
        }

        /* 将已经捕获好视频的内存拉出已捕获视频的队列 */
        if (ioctl(__m_camFd, VIDIOC_DQBUF, &captureBuf) < 0)
        {
            return {false, "video dqbuf failed"};
        }

        if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            if(dataCb)
            {
                dataCb(static_cast<uint8_t*>(__m_buffers[captureBuf.index].buffer), __m_buffers[captureBuf.index].length);
            }
        }
        else if(__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            for(int i = 0; i < numOfPlanes; i++)
            {
                if(dataCb)
                {
                    dataCb(static_cast<uint8_t*>(__m_buffers[captureBuf.index].pstart->start), __m_tmpPlane[i].bytesused);
                }
            }
        }

        if (ioctl(__m_camFd, VIDIOC_QBUF, &captureBuf) == -1)
        {
            return {false, "video qbuf failed"};
        }

        return true;
    }

    bool V4l2Cam::__prepareBuffer()
    {
        enum v4l2_buf_type type;       
        // 把四个缓冲帧放入队列
        for (int i = 0; i < __m_buffers.size(); i++)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = __m_type;
            buf.memory = __m_memoryType;
            buf.index = i;
            if(__m_memoryType == V4L2_MEMORY_MMAP)
            {
                buf.m.offset = __m_offset[i];
            }
            else if(__m_memoryType == V4L2_MEMORY_USERPTR)
            {
                buf.m.userptr = (unsigned long)__m_buffers[i].buffer;
                buf.length = __m_buffers[i].length;
            }
            // 将空闲的内存加入可捕获视频的队列
            if (ioctl(__m_camFd, VIDIOC_QBUF, &buf) < 0)
            {
                printf("ERROR: VIDIOC_QBUF[%s], FUNC[%s], LINE[%d]\n", __m_devName.c_str(), __FUNCTION__, __LINE__);
                return false;
            }
        }
        return true;
    }

    bool V4l2Cam::__prepareBufferMplane()
    {
        int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
        for (int i = 0; i < __m_buffers.size(); i++)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = __m_type;
            buf.memory = __m_memoryType;
            buf.m.planes = static_cast<struct v4l2_plane *>(__m_buffers[i].buffer);
            buf.length = numOfPlanes;
            buf.index = i;

            if (ioctl(__m_camFd, VIDIOC_QBUF, &buf) < 0)
            {
                fprintf(stderr, "VIDIOC QBUF failed.\n");
                return false;
            }
        }
        return true;
    }

    bool V4l2Cam::__freeBufferMplane()
    {
        int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
        if (__m_memoryType == V4L2_MEMORY_MMAP)
        {
            for (int i = 0; i < __m_buffers.size(); i++)
            {
                for (int j = 0; j < numOfPlanes; j++)
                {

                    munmap(__m_buffers[i].pstart[j].start,
                           (static_cast<struct v4l2_plane *>(__m_buffers[j].buffer))[j].length);
                }
                free(__m_buffers[i].pstart);
                free(__m_buffers[i].buffer);
            }

            __m_buffers.clear();
        }

        return true;
    }


    void V4l2Cam::__initMmap(void)
    {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = __m_numOfBuffers;
        req.type = __m_type;
        req.memory = __m_memoryType;
        if (-1 == ioctl(__m_camFd, VIDIOC_REQBUFS, &req))
        {
            if (EINVAL == errno)
            {
                fprintf(stderr, "%s does not support "
                                "memory mapping\n",
                        __m_devName);
                exit(EXIT_FAILURE);
            }
            else
            {
                exit(EXIT_FAILURE);
            }
        }

        if (req.count < 2)
        {
            fprintf(stderr, "Insufficient buffer memory on %s\n",
                    __m_devName);
            exit(EXIT_FAILURE);
        }
        __m_reqCnt = req.count;
        __m_buffers.resize(__m_reqCnt);
        if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            struct v4l2_buffer buf;
            for (int i = 0; i < req.count; i++)
            {
                memset(&buf, 0, sizeof(buf));
                buf.type = __m_type;
                buf.memory = __m_memoryType;
                buf.index = i;

                if (-1 == ioctl(__m_camFd, VIDIOC_QUERYBUF, &buf))
                    exit(EXIT_FAILURE);

                __m_buffers[i].length = buf.length;
                __m_buffers[i].buffer =
                    mmap(NULL /* start anywhere */,
                         buf.length,
                         PROT_READ | PROT_WRITE /* required */,
                         MAP_SHARED /* recommended */,
                         __m_camFd, buf.m.offset);

                if (MAP_FAILED == __m_buffers[i].buffer)
                    exit(EXIT_FAILURE);
            }
        }
        else if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            for (int i = 0; i < req.count; i++)
            { // 映射所有的缓存
                struct v4l2_plane *planesBuffer =
                    static_cast<struct v4l2_plane *>(
                        calloc(numOfPlanes, sizeof(struct v4l2_plane)));
                struct PlaneStart *planeStart =
                    static_cast<struct PlaneStart *>(
                        calloc(numOfPlanes, sizeof(*planeStart)));
                struct v4l2_buffer buf;
                memset(&buf, 0, sizeof(buf));

                buf.type = __m_type;
                buf.memory = __m_memoryType;
                buf.m.planes = planesBuffer;
                buf.length = numOfPlanes;
                buf.index = i;
                if (ioctl(__m_camFd, VIDIOC_QUERYBUF, &buf) == -1)
                { // 获取到对应index的缓存信息，此处主要利用length信息及offset信息来完成后面的mmap操作。
                    fprintf(stderr, "Query buffer failed: %d\n", i);
                    return;
                }

                __m_buffers[i].buffer = planesBuffer;
                __m_buffers[i].pstart = planeStart;
                __m_buffers[i].length = buf.length;

                for (int j = 0; j < numOfPlanes; j++)
                {
                    // 转换成相对地址
                    planeStart[j].start = mmap(nullptr, planesBuffer[j].length,
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED,
                                               __m_camFd, planesBuffer[j].m.mem_offset);
                    if (MAP_FAILED == planeStart[j].start)
                    {
                        printf("mmap failed.\n");
                        return;
                    }
                }
            }
        }
    }
    void V4l2Cam::__initUsrPtr(unsigned int bufferSize,void *usrPtr)
    {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = __m_numOfBuffers;
        req.type = __m_type;
        req.memory = __m_memoryType;
        if (-1 == ioctl(__m_camFd, VIDIOC_REQBUFS, &req))
        {
            if (EINVAL == errno)
            {
                fprintf(stderr, "%s does not support "
                                "user pointer i/o\n",
                        __m_devName);
            }
            else
            {
                exit(EXIT_FAILURE);
            }
        }
        __m_buffers.resize(__m_numOfBuffers);
        if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            for (int i=0; i < __m_numOfBuffers; i++)
            {
                __m_buffers[i].length = bufferSize;
                __m_buffers[i].buffer =(usrPtr + i*bufferSize) ;  //malloc(bufferSize);
                fprintf(stderr,"sizeof usrPtr %u    %u\n", usrPtr,__m_buffers[i].buffer);
                if (!__m_buffers[i].buffer)
                {
                    fprintf(stderr, "Out of memory\n");
                    fprintf(stderr,"%u bufferSize\n",i);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (__m_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            int numOfPlanes = __m_v4l2Fmt.fmt.pix_mp.num_planes;
            for (int i = 0; i < __m_numOfBuffers; i++)
            { 
                struct v4l2_plane *planesBuffer =
                    static_cast<struct v4l2_plane *>(
                        calloc(numOfPlanes, sizeof(struct v4l2_plane)));
                struct PlaneStart *planeStart =
                    static_cast<struct PlaneStart *>(
                        calloc(numOfPlanes, sizeof(*planeStart)));
                //planesBuffer.m.userptr = usrPtr + i* planesBuffer.length;
                __m_buffers[i].buffer = planesBuffer;
                __m_buffers[i].pstart = planeStart;
                __m_buffers[i].length = numOfPlanes;
                for (int j = 0; j < numOfPlanes; j++)
                {
                    // 转换成相对地址
                    planeStart[j].start = usrPtr + j*planesBuffer[j].length;
                    if (!planeStart[j].start)
                    {
                       fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }

} // namespace v4l2_cam