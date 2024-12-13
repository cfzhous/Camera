#include "v4l2_cam/camera.hpp"

namespace V4L2Camera
{
    v4l2Cam::v4l2Cam()
    {
    }

    v4l2Cam::~v4l2Cam()
    {
        m_cam_publisher.reset();
    }
    bool v4l2Cam::initParam(const CamParams &param)
    {
        mCamPara = param;
        return true;
    }
    bool v4l2Cam::open()
    {
        sal::SensorLogger::info("v4l2_cam - open device name: {}.", mCamPara.cam_name);
        if(!m_vl42_cam->open(mCamPara.cam_name, mCamPara.pixel_w, mCamPara.pixel_h, mCamPara.pix_fmt))
        {
            m_cam_status->data.base.working_status = sal::data::LifecycleStatus::SENSOR_START_ERROR;
            m_cam_status->update();
            sal::SensorLogger::error("v4l2_cam - Device open failed: {0}", mCamPara.cam_name);                                
            return;
        }
        sal::SensorLogger::info("v4l2_cam - Device open suc: {0}", mCamPara.cam_name);
        if(mCamPara.enable_usrPtr)
        {
            usrPtr = malloc(m_vl42_cam->bufferSize());
        }        
        if (!m_vl42_cam->initDevice(usrPtr))
        {
            m_cam_status->data.base.working_status = sal::data::LifecycleStatus::SENSOR_START_ERROR;
            m_cam_status->update();
            sal::SensorLogger::error("v4l2_cam - Device usrptr init failed");
            return;
        }
        if (mCamPara.trigger_mode)
        {
            // 如果使能了触发模式，那么就订阅时间戳
            m_ts_sub = awe::Subscriber::create<sal::data::TimestampPlus>(mCamPara.timestamp_topic,
                                                                          std::bind(&v4l2Cam::onTimestampCb, this, std::placeholders::_1));
        }
        sal::SensorLogger::info("v4l2_cam - Device  open sucess {0}", mCamPara.cam_name);
        m_cam_status->data.base.working_status = sal::data::LifecycleStatus::SENSOR_START;
        m_cam_status->update();
    }
    void v4l2Cam::release()
    {
        m_vl42_cam->close();
        m_cam_status->data.base.working_status = sal::data::LifecycleStatus::SENSOR_CLOSE;
        m_cam_status->update();
        if (usrPtr)
        {
            free(usrPtr);
            usrPtr = nullptr;
        }    
    }
    void v4l2Cam::publish()
    {
      
        while (awe::ok())
        {
            m_vl42_cam->capture([&](const uint8_t *data, size_t len) {    
            if (mCamPara.trigger_mode)
            {
            	usleep(2000);
            	int cnt = 5;
            	while(cnt--)
            	{
                    {
                    std::lock_guard<std::mutex> lock(m_tm_mutex);
                    if (m_time_stamp != 0)
                    {
                        // uint64_t time_stamp = m_time_stamp / 1000;
                        //if (m_time_stamp % 100000 != 0)
                       // {
                            // sal::SensorLogger::info("cam publish image in 10 fps - timevalue: {0}.", m_time_stamp);
                       //     return;
                       // }
                       // sal::SensorLogger::info("cam publish image in 10 fps - timevalue: {0}.", m_time_stamp);
                        m_image.setTimestampUSec(m_time_stamp);                       
                        break;
                    }
                    }
                    
                    usleep(2000);
                    
                }
                
                if (m_time_stamp == 0)
                {
                    sal::SensorLogger::warn("m_time_stamp == 0");
                    return;
                }
                m_time_stamp = 0;
                
            }
            else
            {
                
                //uint64_t time_stamp = awe::DateTime::currentDateTime().toUSecsSinceEpoch();
                //sal::SensorLogger::info("cam publish image with systime{}.",time_stamp);
                //m_image.setTimestampUSec(time_stamp);
                m_image.setTimestamp(awe::DateTime::currentDateTime());               
            }
            cv::Mat src_mat(mCamPara.pixel_h,mCamPara.pixel_w,CV_8UC2,(void*)data);    //需要知道原始数据格式  
            //mCap.set(cv::CAP_PROP_CONVERT_RGB, 0); //由于opencv默认将YUV转换成BGR格式，而森云USB原始数据是UYVY，需要关闭格式转换，拿到原始的UYVY数据  wjx        
            // sal::SensorLogger::info("cv::mat - yuvmat: {0}", src_mat.type());
            cv::Mat dst_mat;
            if(mCamPara.pix_fmt == V4L2_PIX_FMT_YUYV) 
            {
                cv::cvtColor(src_mat,dst_mat,cv::COLOR_YUV2BGR_YUYV);  //数据格式转换
            }
            else if(mCamPara.pix_fmt == V4L2_PIX_FMT_UYVY) 
            {
                cv::cvtColor(src_mat,dst_mat,cv::COLOR_YUV2BGR_UYVY);    // 
            }
            // cv::imshow("Camera", dst_mat);
            // cv::waitKey(10);
            m_image.fromCvMat(dst_mat);
            m_cam_publisher->publish(m_image);
            m_cam_status->data.base.working_status = sal::data::LifecycleStatus::SENSOR_RUNNING;
            m_cam_status->data.topic_status.heartbeat();
            m_cam_status->update();
            });
        }
    }
    void v4l2Cam::onTimestampCb(const sal::data::TimestampPlus &msg)
    {
        int index = (mCamPara.ts_group - 1) * 4 + mCamPara.ts_channal;
        if (index > 12 || index < 1)
        {
            sal::SensorLogger::warn("--- ts group({}) or channal({}) error.", mCamPara.ts_group, mCamPara.ts_channal);
        }
        int index_start = (msg.group - 1) * 4 + msg.channel_start_id;
        int index_end = (msg.group - 1) * 4 + msg.channel_start_id + msg.channel_num;

        if (index >= index_start && index_start <= index_end)
        {
            std::lock_guard<std::mutex> lock(m_tm_mutex);
            m_time_stamp = msg.timeValueUSec();  
        }
    }
}
