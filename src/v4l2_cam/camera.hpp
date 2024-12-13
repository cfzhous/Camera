#pragma once


#include <memory>
#include <pthread.h>
#include <thread>

#include "v4l2_cam/v4l2_api.hpp"

namespace V4L2Camera
{
    struct CamParams
    {
        /* data */
        std::string cam_name;
        std::string timestamp_topic;
        std::string frame_id;
        int pixel_w;
        int pixel_h;
        uint32_t pix_fmt;
        std::string topic_name;
        bool enable_usrPtr;
        bool trigger_mode;
        int ts_group;
        int ts_channal;
    };

    class v4l2Cam
    {
    public:
        v4l2Cam();
        ~v4l2Cam();
        bool initParam(const CamParams &param);
        bool open();
        void publish();
        void release();
        /**
         * @brief 自检
         * @return true
         * @return false
         */
        bool selfCheck();

        /**
         * @brief 启动
         * @return true
         * @return false
         */

    private:
        CamParams mCamPara;
        uint64_t m_time_stamp=0;
        void *usrPtr = nullptr;
        // 定义 pub 对象
        std::shared_ptr<awe::Publisher> m_cam_publisher;
        sal::data::Image m_image;
        std::mutex m_tm_mutex;
        std::shared_ptr<v4l2_api::V4l2Cam> m_vl42_cam = std::make_shared<v4l2_api::V4l2Cam>();
        std::shared_ptr<awe::Subscriber> m_ts_sub;
        std::shared_ptr<sal::data::Status<sal::data::CameraStatus>> m_cam_status;
    
    private:
        void onTimestampCb(const sal::data::TimestampPlus &msg);   //设置时间同步

    };
}