
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>         // std::mutex, std::unique_lock
#include <cmath>
#include <iostream>

#include "MacAddr.h"



#if 1 //tracking


#define TRUE 1
#define FALSE 0

#include "CTrackingInterfaceUdpSocket.h"
CTrackingInterfaceUdpSocket g_trackingInterfaceUdpSocket;
int g_groupNum = 1;
BOOL g_trackingOnOff = 1;
int init_left = 0, init_right = 0, init_top = 0, init_bottom = 0;
int tracking_left = 0, tracking_right = 0, tracking_top = 0, tracking_bottom = 0;

#pragma comment(lib, "opencv_tracking342")

enum TRACKING_STATUS
{
    TRACKING_IDLE = 0,
    TRACKING_START = 1,
    TRACKING_ING = 2,
    TRACKING_FAILURE = 3,
    TRACKING_CANCEL = 4,
};
static enum TRACKING_STATUS g_trackingStatus = TRACKING_IDLE;
#endif

#if 1//ffmpeg


//https://semoa.tistory.com/992
#pragma warning (disable : 4996)

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include "libswscale/swscale.h"

#pragma comment ( lib, "avformat.lib")
#pragma comment ( lib, "swscale.lib" )
#pragma comment ( lib, "avcodec.lib" )
#pragma comment ( lib, "avutil.lib" )
#pragma comment ( lib, "avdevice.lib" )
}

#define INBUF_SIZE 4096

AVFrame *frame = NULL;
AVFormatContext* input_ctx = NULL;
AVPacket packet;

int video_stream_index;
int audio_stream_index;

AVCodec *pCodec = NULL;
AVCodecContext *video_decoder_ctx;
AVCodecContext *audio_decoder_ctx;

SwsContext * m_swsctxToBGR24 = NULL;


#endif



std::string ReplaceAll(std::string &str, const std::string& from, const std::string& to) {
    size_t start_pos = 0; //string처음부터 검사
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)  //from을 찾을 수 없을 때까지
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // 중복검사를 피하고 from.length() > to.length()인 경우를 위해서
    }
    return str;
}
//

// It makes sense only for video-Camera (not for video-File)
// To use - uncomment the following line. Optical-flow is supported only by OpenCV 3.x - 4.x
//#define TRACK_OPTFLOW
//#define GPU

// To use 3D-stereo camera ZED - uncomment the following line. ZED_SDK should be installed.
//#define ZED_STEREO


#include "yolo_v2_class.hpp"    // imported functions from DLL


cv::Mat avframe_to_cvmat(AVFrame *frame)
{

    AVFrame dst;
    cv::Mat m;

    memset(&dst, 0, sizeof(dst));

    int w = frame->width, h = frame->height;
    m = cv::Mat(h, w, CV_8UC3);
    dst.data[0] = (uint8_t *)m.data;


    avpicture_fill((AVPicture *)&dst, dst.data[0], AV_PIX_FMT_BGR24, w, h);

  
    sws_scale(m_swsctxToBGR24, frame->data, frame->linesize, 0, frame->height,
        dst.data, dst.linesize);
    return m;


}
/*
int decode_write(AVCodecContext * avctx, AVPacket * packet, cv::Mat & srcMat)
{
    AVFrame *frame = NULL;// , *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
   // uint8_t *buffer = NULL;
    int size;
    int ret = 0;

    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }

    while (1) {
        if (!(frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            //av_frame_free(&sw_frame);
            return ret;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto fail;
        }

        srcMat = avframe_to_cvmat(frame);
        
    fail:
        av_frame_free(&frame);
       // av_frame_free(&sw_frame);
        //av_freep(&buffer);
        if (ret < 0)
            return ret;
    }
}
*/
int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = packet.size;
    *got_frame = 0;
    if (packet.stream_index == video_stream_index) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_decoder_ctx, frame, got_frame, &packet);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame\n");
            return ret;
        }
        if (*got_frame) {
            //printf("video_frame%s n:%d coded_n:%d pts:%s\n",
            //    cached ? "(cached)" : "",
            //    video_frame_count++, frame->coded_picture_number,
            //    av_ts2timestr(frame->pts, &video_dec_ctx->time_base));
            ///* copy decoded frame to destination buffer:
            // * this is required since rawvideo expects non aligned data */
            //av_image_copy(video_dst_data, video_dst_linesize,
            //    (const uint8_t **)(frame->data), frame->linesize,
            //    video_dec_ctx->pix_fmt, video_dec_ctx->width, video_dec_ctx->height);
            ///* write to rawvideo file */
            //fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    }
    else if (packet.stream_index == audio_stream_index) {
        *got_frame = 1;
        /* decode audio frame */
    //    ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &packet);
    //    if (ret < 0) {
    //        fprintf(stderr, "Error decoding audio frame\n");
    //        return ret;
    //    }
    //    /* Some audio decoders decode only part of the packet, and have to be
    //     * called again with the remainder of the packet data.
    //     * Sample: fate-suite/lossless-audio/luckynight-partial.shn
    //     * Also, some decoders might over-read the packet. */
    //    decoded = FFMIN(ret, packet.size);
    //    if (*got_frame) {
    //        size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
    //        printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
    //            cached ? "(cached)" : "",
    //            audio_frame_count++, frame->nb_samples,
    //            av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
    //        /* Write the raw audio data samples of the first plane. This works
    //         * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
    //         * most audio decoders output planar audio, which uses a separate
    //         * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
    //         * In other words, this code will write only the first audio channel
    //         * in these cases.
    //         * You should use libswresample or libavfilter to convert the frame
    //         * to packed data. */
    //        fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
    //    }
    }
    return decoded;
}
std::vector<bbox_t> ProcessTracking(cv::Mat &tempMat)
{
    int trackingVideoWidth = tempMat.cols;
    int trackingVideoHeight = tempMat.rows;
    static int recvWidth = 0;
    static int recvHeight = 0;
    std::vector<bbox_t> result;

    if (g_trackingInterfaceUdpSocket.IsRecvData() == TRUE)
    {
        int recvTrackingStatus = 0;
        int recv_groupNum = 0;
        

        string recvData = g_trackingInterfaceUdpSocket.PopRecvData();
        g_trackingInterfaceUdpSocket.ParseInterfaceFormat(recvData,
            recv_groupNum,
            init_left, init_right, init_top, init_bottom, recvWidth, recvHeight,
            recvTrackingStatus);



        float ratio_width = (float)trackingVideoWidth / (float)recvWidth;
        float ratio_height = (float)trackingVideoHeight / (float)recvHeight;

        init_left *= ratio_width;
        init_right *= ratio_width;
        init_top *= ratio_height;
        init_bottom *= ratio_height;

        if (recv_groupNum == g_groupNum)
        {
            g_trackingStatus = (TRACKING_STATUS)recvTrackingStatus;
        }
    }

    //시간측정 위한 코드
    //double time = what_time_is_it_now();//
    //
    static int failDispCnt = 0;
    switch (g_trackingStatus)
    {
    case TRACKING_IDLE:
    {
        tracking_left = 0;
        tracking_right = 0;
        tracking_top = 0;
        tracking_bottom = 0;

        failDispCnt = 0;
    }
    break;
    case TRACKING_START://트랙킹 시작
    {
        BOOL ret = init_tracker(tempMat, init_left, init_right, init_top, init_bottom);

        if (ret == TRUE)
        {
            g_trackingStatus = TRACKING_ING;
        }
        else
        {
            g_trackingStatus = TRACKING_FAILURE;
        }
    }
    break;
    case TRACKING_ING:
    {
        BOOL ret = update_tracking_info(tempMat, &tracking_left, &tracking_right, &tracking_top, &tracking_bottom);
        
        //원본사이즈로
        float ratio_width = (float)recvWidth /(float)trackingVideoWidth;
        float ratio_height = (float)recvHeight / (float)trackingVideoHeight;

        bbox_t box;
        box.track_id = 1;
        box.x = tracking_left;
        box.y = tracking_top;
        box.w = tracking_right - tracking_left;
        box.h = tracking_bottom - tracking_top;
        box.x *= ratio_width;
        box.y *= ratio_height;
        box.w *= ratio_width;
        box.h *= ratio_height;

        result.push_back(box);

        if (ret == TRUE)
        {
            g_trackingStatus = TRACKING_ING;
        }
        else
        {
            g_trackingStatus = TRACKING_FAILURE;
        }
    }
    break;
    case TRACKING_FAILURE:
    {
        if (failDispCnt >= 30)
        {
            g_trackingStatus = TRACKING_IDLE;
        }
        failDispCnt++;

    }
    break;

    default:
        break;
    }

    string packet = g_trackingInterfaceUdpSocket.MakeInterfaceFormat(
        g_groupNum, tracking_left, tracking_right, tracking_top, tracking_bottom,
        trackingVideoWidth, trackingVideoHeight,
        g_trackingStatus);
    g_trackingInterfaceUdpSocket.SetSendData(packet);

    return result;
}





#ifdef OPENCV
#ifdef ZED_STEREO
#include <sl/Camera.hpp>
#if ZED_SDK_MAJOR_VERSION == 2
#define ZED_STEREO_2_COMPAT_MODE
#endif

#undef GPU // avoid conflict with sl::MEM::GPU

#ifdef ZED_STEREO_2_COMPAT_MODE
#pragma comment(lib, "sl_core64.lib")
#pragma comment(lib, "sl_input64.lib")
#endif
#pragma comment(lib, "sl_zed64.lib")

float getMedian(std::vector<float> &v) {
    size_t n = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + n, v.end());
    return v[n];
}

std::vector<bbox_t> get_3d_coordinates(std::vector<bbox_t> bbox_vect, cv::Mat xyzrgba)
{
    bool valid_measure;
    int i, j;
    const unsigned int R_max_global = 10;

    std::vector<bbox_t> bbox3d_vect;

    for (auto &cur_box : bbox_vect) {

        const unsigned int obj_size = std::min(cur_box.w, cur_box.h);
        const unsigned int R_max = std::min(R_max_global, obj_size / 2);
        int center_i = cur_box.x + cur_box.w * 0.5f, center_j = cur_box.y + cur_box.h * 0.5f;

        std::vector<float> x_vect, y_vect, z_vect;
        for (int R = 0; R < R_max; R++) {
            for (int y = -R; y <= R; y++) {
                for (int x = -R; x <= R; x++) {
                    i = center_i + x;
                    j = center_j + y;
                    sl::float4 out(NAN, NAN, NAN, NAN);
                    if (i >= 0 && i < xyzrgba.cols && j >= 0 && j < xyzrgba.rows) {
                        cv::Vec4f &elem = xyzrgba.at<cv::Vec4f>(j, i);  // x,y,z,w
                        out.x = elem[0];
                        out.y = elem[1];
                        out.z = elem[2];
                        out.w = elem[3];
                    }
                    valid_measure = std::isfinite(out.z);
                    if (valid_measure)
                    {
                        x_vect.push_back(out.x);
                        y_vect.push_back(out.y);
                        z_vect.push_back(out.z);
                    }
                }
            }
        }

        if (x_vect.size() * y_vect.size() * z_vect.size() > 0)
        {
            cur_box.x_3d = getMedian(x_vect);
            cur_box.y_3d = getMedian(y_vect);
            cur_box.z_3d = getMedian(z_vect);
        }
        else {
            cur_box.x_3d = NAN;
            cur_box.y_3d = NAN;
            cur_box.z_3d = NAN;
        }

        bbox3d_vect.emplace_back(cur_box);
    }

    return bbox3d_vect;
}

cv::Mat slMat2cvMat(sl::Mat &input) {
    int cv_type = -1; // Mapping between MAT_TYPE and CV_TYPE
    if(input.getDataType() ==
#ifdef ZED_STEREO_2_COMPAT_MODE
        sl::MAT_TYPE_32F_C4
#else
        sl::MAT_TYPE::F32_C4
#endif
        ) {
        cv_type = CV_32FC4;
    } else cv_type = CV_8UC4; // sl::Mat used are either RGBA images or XYZ (4C) point clouds
    return cv::Mat(input.getHeight(), input.getWidth(), cv_type, input.getPtr<sl::uchar1>(
#ifdef ZED_STEREO_2_COMPAT_MODE
        sl::MEM::MEM_CPU
#else
        sl::MEM::CPU
#endif
        ));
}

cv::Mat zed_capture_rgb(sl::Camera &zed) {
    sl::Mat left;
    zed.retrieveImage(left);
    cv::Mat left_rgb;
    cv::cvtColor(slMat2cvMat(left), left_rgb, CV_RGBA2RGB);
    return left_rgb;
}

cv::Mat zed_capture_3d(sl::Camera &zed) {
    sl::Mat cur_cloud;
    zed.retrieveMeasure(cur_cloud,
#ifdef ZED_STEREO_2_COMPAT_MODE
        sl::MEASURE_XYZ
#else
        sl::MEASURE::XYZ
#endif
        );
    return slMat2cvMat(cur_cloud).clone();
}

static sl::Camera zed; // ZED-camera

#else   // ZED_STEREO
std::vector<bbox_t> get_3d_coordinates(std::vector<bbox_t> bbox_vect, cv::Mat xyzrgba) {
    return bbox_vect;
}
#endif  // ZED_STEREO


#include <opencv2/opencv.hpp>            // C++
#include <opencv2/core/version.hpp>
#ifndef CV_VERSION_EPOCH     // OpenCV 3.x and 4.x
#include <opencv2/videoio/videoio.hpp>
#include "yolo_console_dll.h"
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_MAJOR)"" CVAUX_STR(CV_VERSION_MINOR)"" CVAUX_STR(CV_VERSION_REVISION)
#ifndef USE_CMAKE_LIBS
#pragma comment(lib, "opencv_world" OPENCV_VERSION ".lib")
#ifdef TRACK_OPTFLOW
/*
#pragma comment(lib, "opencv_cudaoptflow" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_cudaimgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_core" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_imgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_highgui" OPENCV_VERSION ".lib")
*/
#endif    // TRACK_OPTFLOW
#endif    // USE_CMAKE_LIBS
#else     // OpenCV 2.x
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_EPOCH)"" CVAUX_STR(CV_VERSION_MAJOR)"" CVAUX_STR(CV_VERSION_MINOR)
#ifndef USE_CMAKE_LIBS
#pragma comment(lib, "opencv_core" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_imgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_highgui" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_video" OPENCV_VERSION ".lib")
#endif    // USE_CMAKE_LIBS
#endif    // CV_VERSION_EPOCH


void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names, std::vector<bbox_t> result_tracking_vec,
    int current_det_fps = -1, int current_cap_fps = -1, int current_tracking_fps = -1)
{
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

    for (auto &i : result_vec) {
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        if (obj_names.size() > i.obj_id) {
            std::string obj_name = obj_names[i.obj_id];

#if 0
            if (i.track_id > 0)
                obj_name += " - " + std::to_string(i.track_id);
#else
           
#endif
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
            int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
            max_width = std::max(max_width, (int)i.w + 2);
            //max_width = std::max(max_width, 283);
            std::string coords_3d;
            if (!std::isnan(i.z_3d)) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << "x:" << i.x_3d << "m y:" << i.y_3d << "m z:" << i.z_3d << "m ";
                coords_3d = ss.str();
                cv::Size const text_size_3d = getTextSize(ss.str(), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, 1, 0);
                int const max_width_3d = (text_size_3d.width > i.w + 2) ? text_size_3d.width : (i.w + 2);
                if (max_width_3d > max_width) max_width = max_width_3d;
            }

            cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 35, 0)),
                cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
                color, CV_FILLED, 8, 0);
            putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
            if(!coords_3d.empty()) putText(mat_img, coords_3d, cv::Point2f(i.x, i.y-1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 1);
        }
    }

    //draw tracking box
    for (auto &i : result_tracking_vec) {
        cv::Scalar color = obj_id_to_color(i.track_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        if (i.track_id > 0)
        {
            std::string obj_name = "Tracking " + std::to_string(i.track_id);
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
            int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
            max_width = std::max(max_width, (int)i.w + 2);
            cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 35, 0)),
                cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
                color, CV_FILLED, 8, 0);
            putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
        }
    }

    if (current_det_fps >= 0 && current_cap_fps >= 0 || current_tracking_fps >= 0)
    {
        std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS tracking: " + std::to_string(current_tracking_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
        putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(50, 255, 0), 2);
    }
}
#endif    // OPENCV


void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names, int frame_id = -1) {
    if (frame_id >= 0) std::cout << " Frame: " << frame_id << std::endl;
    for (auto &i : result_vec) {
        if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << " - ";
        std::cout << "obj_id = " << i.obj_id << ",  x = " << i.x << ", y = " << i.y
            << ", w = " << i.w << ", h = " << i.h
            << std::setprecision(3) << ", prob = " << i.prob << std::endl;
    }
}

std::vector<std::string> objects_names_from_file(std::string const filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open()) return file_lines;
    for(std::string line; getline(file, line);) file_lines.push_back(line);
    std::cout << "object names loaded \n";
    return file_lines;
}

template<typename T>
class send_one_replaceable_object_t {
    const bool sync;
    std::atomic<T *> a_ptr;
public:

    void send(T const& _obj) {
        T *new_ptr = new T;
        *new_ptr = _obj;
        if (sync) {
            while (a_ptr.load()) std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        std::unique_ptr<T> old_ptr(a_ptr.exchange(new_ptr));
    }

    T receive() {
        std::unique_ptr<T> ptr;
        do {
            while(!a_ptr.load()) std::this_thread::sleep_for(std::chrono::milliseconds(3));
            ptr.reset(a_ptr.exchange(NULL));
        } while (!ptr);
        T obj = *ptr;
        return obj;
    }

    bool is_object_present() {
        return (a_ptr.load() != NULL);
    }

    send_one_replaceable_object_t(bool _sync) : sync(_sync), a_ptr(NULL)
    {}
};

//메인함수
int main(int argc, char *argv[])
{
    bool authSuccess = false;
    vector<string> macAddrss = getMacAddress();
    for (int i = 0; i < macAddrss.size(); i++)
    {
        //00-01-29-9C-4C-E8
        //0001299C4CE8
        if ( macAddrss[i] == "0001299C4CE8" )//myHomeAiCom
        {
            authSuccess = true;
        }
        //00-01-29-9C-4C-E6
        //0001299C4CE6
        if (macAddrss[i] == "0001299C4CE6")//u2 ai com
        {
            authSuccess = true;
        }
    }

    if (authSuccess == false)
    {
        cout << "No Authorization" << endl;
        Sleep(10000);
        exit(1);
    }

    std::string  names_file = "data/coco.names";    
    std::string  cfg_file = "cfg/yolov3.cfg";    
    std::string  weights_file = "yolov3.weights";
    std::string filename;  

    if (argc > 4) {    //voc.names yolo-voc.cfg yolo-voc.weights test.mp4
        names_file = argv[1];//1
        cfg_file = argv[2];//2
        weights_file = argv[3];//3
        filename = argv[4];//4
    }
    else if (argc > 1) filename = argv[1];

    //5
    float const thresh = (argc > 5) ? std::stof(argv[5]) : 0.2;

    //6
    int groupNum = (argc > 6) ? std::stof(argv[6]) : 1;

    //7
    int jsonPort = (argc > 7) ? std::stof(argv[7]) : 1011;

    //8
    bool showVideo = (argc > 8) ? std::stof(argv[8]) : true;

    //9
    string chName = (argc > 9) ? argv[9] : "NoName";

    //10
    bool showConsoleWindow = (argc > 10) ? std::stof(argv[10]) : false;

    if (showConsoleWindow == false)
        ::ShowWindow(GetConsoleWindow(), SW_HIDE);

    printf("nameFile=%s, cfgFile=%s, weightsFile=%s, videoFile=%s, thresh=%lf, groupNum=%d, jsonPort=%d, showVieo=%d, chName=%s, showConsoleWindow=%d \r\n",
        names_file, cfg_file, weights_file, filename,
        thresh, groupNum, jsonPort, showVideo, chName, showConsoleWindow);

    avcodec_register_all();
    av_register_all();
    avformat_network_init();

    g_groupNum = groupNum;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    unsigned num_cpus = std::thread::hardware_concurrency();

    cout << info.dwNumberOfProcessors << endl;

    string windowName = chName + "-VideoAnalysis " + to_string(groupNum);
    if (showVideo)
    {
        cv::namedWindow(windowName, cv::WINDOW_NORMAL);
        HWND hWnd = (HWND)cvGetWindowHandle(windowName.c_str());
        hWnd = GetParent(hWnd);
        //SetWindowPos( hWnd, HWND_TOPMOST, 0, 0, 500, 500, SWP_NOSIZE|SWP_NOMOVE);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 500, 500, SWP_NOMOVE);
    }    

    Detector detector(cfg_file, weights_file);

    auto obj_names = objects_names_from_file(names_file);
    std::string out_videofile = "result.avi";
    bool const save_output_videofile = false;   // true - for history
    bool const send_network = true;        // true - for remote detection
    bool const use_kalman_filter = false;   // true - for stationary camera
    
    //bool detection_sync = true;             // true - for video-file
    bool detection_sync = false;             // steven 수정
    bool isFile = true;

#ifdef TRACK_OPTFLOW    // for slow GPU
    detection_sync = false;
    Tracker_optflow tracker_flow;
    //detector.wait_stream = true;
#endif  // TRACK_OPTFLOW


    while (true)
    {
        std::cout << "input image or video filename: ";
        if(filename.size() == 0) std::cin >> filename;
        if (filename.size() == 0) break;

        try {
#ifdef OPENCV
            preview_boxes_t large_preview(100, 150, false), small_preview(50, 50, true);
            bool show_small_boxes = false;

            std::string const file_ext = filename.substr(filename.find_last_of(".") + 1);
            std::string const protocol = filename.substr(0, 7);
            if (file_ext == "avi" || file_ext == "mp4" || file_ext == "mjpg" || file_ext == "mov" ||     // video file
                protocol == "rtmp://" || protocol == "rtsp://" || protocol == "http://" || protocol == "https:/" ||    // video network stream
                filename == "zed_camera" || file_ext == "svo" || filename == "web_camera")   // ZED stereo camera

            {
                if (protocol == "rtsp://" || protocol == "http://" || protocol == "https:/" || filename == "zed_camera" || filename == "web_camera")
                {
                    detection_sync = false;
                    isFile = false;
                }

                detection_sync = false;

                cv::Mat cur_frame;
                std::atomic<int> fps_cap_counter(0), fps_det_counter(0), fps_tracking_counter(0), fps_prepare_counter(0);/*tracking*/
                std::atomic<int> current_fps_cap(0), current_fps_det(0), current_fps_tracking(0), current_fps_prepare(0);/*tracking*/
                std::atomic<bool> exit_flag(false);
                std::chrono::steady_clock::time_point steady_start, steady_end;
                int video_fps = 25;
                bool use_zed_camera = false;

                track_kalman_t track_kalman;

#ifdef ZED_STEREO
                sl::InitParameters init_params;
                init_params.depth_minimum_distance = 0.5;
    #ifdef ZED_STEREO_2_COMPAT_MODE
                init_params.depth_mode = sl::DEPTH_MODE_ULTRA;
                init_params.camera_resolution = sl::RESOLUTION_HD720;// sl::RESOLUTION_HD1080, sl::RESOLUTION_HD720
                init_params.coordinate_units = sl::UNIT_METER;
                init_params.camera_buffer_count_linux = 2;
                if (file_ext == "svo") init_params.svo_input_filename.set(filename.c_str());
    #else
                init_params.depth_mode = sl::DEPTH_MODE::ULTRA;
                init_params.camera_resolution = sl::RESOLUTION::HD720;// sl::RESOLUTION::HD1080, sl::RESOLUTION::HD720
                init_params.coordinate_units = sl::UNIT::METER;
                if (file_ext == "svo") init_params.input.setFromSVOFile(filename.c_str());
    #endif
                //init_params.sdk_cuda_ctx = (CUcontext)detector.get_cuda_context();
                init_params.sdk_gpu_id = detector.cur_gpu_id;

                if (filename == "zed_camera" || file_ext == "svo") {
                    std::cout << "ZED 3D Camera " << zed.open(init_params) << std::endl;
                    if (!zed.isOpened()) {
                        std::cout << " Error: ZED Camera should be connected to USB 3.0. And ZED_SDK should be installed. \n";
                        getchar();
                        return 0;
                    }
                    cur_frame = zed_capture_rgb(zed);
                    use_zed_camera = true;
                }
#endif  // ZED_STEREO

                cv::VideoCapture cap;

                if (1)
                {
                    //ffmpeg
                }
                else if (filename == "web_camera") {
                    cap.open(0);
                    cap >> cur_frame;
                } else if (!use_zed_camera) {
                    cap.open(filename);
                    cap >> cur_frame;
                }
#ifdef CV_VERSION_EPOCH // OpenCV 2.x
                video_fps = cap.get(CV_CAP_PROP_FPS);
#else
                video_fps = cap.get(cv::CAP_PROP_FPS);
#endif
                cv::Size frame_size = cur_frame.size();
                //cv::Size const frame_size(cap.get(CV_CAP_PROP_FRAME_WIDTH), cap.get(CV_CAP_PROP_FRAME_HEIGHT));
                std::cout << "\n Video size: " << frame_size << std::endl;

                cv::VideoWriter output_video;
                if (save_output_videofile)
#ifdef CV_VERSION_EPOCH // OpenCV 2.x
                    output_video.open(out_videofile, CV_FOURCC('D', 'I', 'V', 'X'), std::max(35, video_fps), frame_size, true);
#else
                    output_video.open(out_videofile, cv::VideoWriter::fourcc('D', 'I', 'V', 'X'), std::max(35, video_fps), frame_size, true);
#endif

                struct detection_data_t {
                    cv::Mat cap_frame;
                    std::shared_ptr<image_t> det_image;
                    std::vector<bbox_t> result_vec;
                    cv::Mat draw_frame;
                    bool new_detection;
                    uint64_t frame_id;
                    bool exit_flag;
                    cv::Mat zed_cloud;
                    std::queue<cv::Mat> track_optflow_queue;
                    detection_data_t() : new_detection(false), exit_flag(false) {}
                };

                const bool sync = detection_sync; // sync data exchange
                send_one_replaceable_object_t<detection_data_t> cap2prepare(sync), cap2draw(sync),
                    prepare2detect(sync), detect2draw(sync),
                    prepare2tracking(sync), tracking2draw(sync),//tracking
                    draw2show(sync), draw2write(sync), draw2net(sync);

                std::thread t_cap, t_prepare, t_detect, t_tracking/*tracking*/, t_post, t_draw, t_write, t_network;

                //Tracking Inerface
                g_trackingInterfaceUdpSocket.CreateSocket(
                    "127.0.0.1", 1472,
                    "127.0.0.1", 1473,
                    "TrackingInterfaceUdpSocket");

             
                
                
        
                //
                
                
                // capture new video-frame
                //std::mutex iomutex;
                //unsigned cpuIndex = 4;
               
                if (t_cap.joinable()) t_cap.join();                
                t_cap = std::thread([&]()
                {
                    uint64_t frame_id = 0;
                    detection_data_t detection_data;
                    cv::Mat prev_cap_frame;
                    do {
                        detection_data = detection_data_t();
#ifdef ZED_STEREO
                        if (use_zed_camera) {
                            while (zed.grab() !=
#ifdef ZED_STEREO_2_COMPAT_MODE
                                sl::SUCCESS
#else
                                sl::ERROR_CODE::SUCCESS
#endif
                                ) std::this_thread::sleep_for(std::chrono::milliseconds(2));
                            detection_data.cap_frame = zed_capture_rgb(zed);
                            detection_data.zed_cloud = zed_capture_3d(zed);
                        }
                        else
#endif   // ZED_STEREO

#if 1 //FFmpeg
                        static bool isConnection = false;
                        if (isConnection == false)
                        {
                            cout << "open Video" << endl;
#if 1//해제
                            if (packet.buf != NULL)
                            {   
                                av_init_packet(&packet);
                            }
                            if (input_ctx != NULL)
                            {
                                //av_read_pause(input_ctx);
                                avformat_close_input(&input_ctx);
                            }
                            
#endif
                            //if(avformat_open_input(&context, "rtsp://192.168.0.40/vod/mp4:sample.mp4",NULL,NULL) != 0){ 
                            //if(avformat_open_input(&context, "rtmp://192.168.0.40/vod/sample.mp4",NULL,NULL) != 0){       
                            //if(avformat_open_input(&context, "d:\\windows\\b.mp4",NULL,NULL) != 0){ 
                            /* open the input file */
                            AVDictionary *stream_opts = 0;
                            av_dict_set(&stream_opts, "rtsp_transport", "tcp", 0);//한프로그램에서 스레드 여러개 돌리면 속도가 느려짐?
                                                                                  //av_dict_set(&stream_opts, "rtsp_transport", "http", 0);
                                                                                  //av_dict_set(&stream_opts, "rtsp_flags", "prefer_tcp", 0);
                                                                                  //av_dict_set(&stream_opts, "rtsp_flags", "listen", 0);
                                                                                  //av_dict_set(&stream_opts, "reorder_queue_size", "tcp", 0);

                            av_dict_set(&stream_opts, "stimeout", "10000000", 0); // in microseconds.
                                                                                  //av_dict_set(&stream_opts, "max_alloc ", "10000", 0);
                            //open rtsp 
                            if (avformat_open_input(&input_ctx, filename.c_str(), NULL, &stream_opts) != 0) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }

                            if (avformat_find_stream_info(input_ctx, NULL) < 0) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }
                                              
                            /* find the video stream information */
                            video_stream_index = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
                            if (video_stream_index < 0) {
                                fprintf(stderr, "Cannot find a video stream in the input file\n");
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }

#if 0
                            audio_stream_index = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, video_stream_index, &pCodec, 0);
                            if (audio_stream_index < 0) {
                                fprintf(stderr, "Cannot find a audio stream in the input file\n");
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                             //   continue;
                            }
#endif
                            // Get a pointer to the codec context for the video stream
    //
                            if (!(video_decoder_ctx = avcodec_alloc_context3(pCodec)))
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }
                      
                            AVStream* video = input_ctx->streams[video_stream_index];
                            if (avcodec_parameters_to_context(video_decoder_ctx, video->codecpar) < 0)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }


                            // Find the decoder for the video stream
                            pCodec = avcodec_find_decoder(video_decoder_ctx->codec_id);
                            if (pCodec == NULL) {
                                fprintf(stderr, "Unsupported codec!\n");
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }
                            // Open codec
                            //if(avcodec_open(pCodecCtx, pCodec)<0)
                            if (avcodec_open2(video_decoder_ctx, pCodec, NULL) < 0)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }

                            if (!(frame = av_frame_alloc())) {
                                fprintf(stderr, "Can not alloc frame\n");
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                continue;
                            }

                            //start reading packets from stream and write them to file 
                            //av_read_play(input_ctx);//play RTSP

                            enum AVPixelFormat src_pixfmt = AV_PIX_FMT_YUV420P;
                            enum AVPixelFormat dst_pixfmt = AV_PIX_FMT_BGR24;
                            m_swsctxToBGR24 = sws_getContext(video_decoder_ctx->width, video_decoder_ctx->height, src_pixfmt,
                                video_decoder_ctx->width, video_decoder_ctx->height, dst_pixfmt,
                                SWS_FAST_BILINEAR, NULL, NULL, NULL);
                            frame_size.width = video_decoder_ctx->width;
                            frame_size.height = video_decoder_ctx->height;
                            isConnection = true;
                        }

                        //프레임 읽기     
                        int ret = 0, got_frame;
                        static int skip_cnt = 0;

                        if (isFile)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }

                        if ((ret = av_read_frame(input_ctx, &packet)) < 0)
                        {
                            //확인 필요
                            static int failCnt = 0;

                            if (failCnt > 30)
                            {
                                failCnt = 0;
                                isConnection = false;
                            }
                            printf("av_read_frame fail \r\n");
                            failCnt++;
                            continue;
                        }
                        else
                        {
                            /*if (video_stream_index == packet.stream_index)
                            {*/
                                if (skip_cnt > 0)
                                {
                                    skip_cnt--;
                                    std::cout << " skip frame " << skip_cnt << "\n";
                                    detection_data.cap_frame = prev_cap_frame;
                                }
                                else
                                {
                                    ret = decode_packet(&got_frame, 0);
                                    if (ret > 0 && got_frame == 1 )
                                    {                                        
                                        auto start = std::chrono::steady_clock::now();                                    
                                        prev_cap_frame = detection_data.cap_frame = avframe_to_cvmat(frame);

                                        auto end = std::chrono::steady_clock::now();
                                        std::chrono::duration<double> spent = end - start;
                                        int ms = spent.count() * 1000;
                                        if (ms > 20)
                                        {
                                            std::cout << " Time: " << spent.count() * 1000 << " ms \n";
                                            skip_cnt++;
                                        }
                                    }
                                    else
                                    {
                                        //cout << "decode fail" << endl;
                                        av_packet_unref(&packet);
                                        continue;
                                    }
                                }
                            /*}
                            else
                            {
                                cout << "not video" << endl;
                            }*/
                            av_packet_unref(&packet);
                            
                        }
                        //end ffmpeg
#else
                        {
                            cap >> detection_data.cap_frame;
                        }
#endif
                        //printf("capture thread cpu = %d\n", GetCurrentProcessorNumber());
                        fps_cap_counter++;
                        detection_data.frame_id = frame_id++;
                        if (detection_data.cap_frame.empty() || exit_flag) {
                            std::cout << " exit_flag: detection_data.cap_frame.size = " << detection_data.cap_frame.size() << std::endl;
                            detection_data.exit_flag = true;
                            detection_data.cap_frame = cv::Mat(frame_size, CV_8UC3);
                        }

                        if (!detection_sync) {
                            cap2draw.send(detection_data);       // skip detection
                        }
                        cap2prepare.send(detection_data);
                    } while (!detection_data.exit_flag);
                    std::cout << " t_cap exit \n";
                });
           

                // pre-processing video frame (resize, convertion)
                //비디오 프레임 전처리 과정
                t_prepare = std::thread([&]()
                {
                    std::shared_ptr<image_t> det_image;
                    std::shared_ptr<image_t> det_image_tracking;
                    detection_data_t detection_data;
                    detection_data_t tracking_data;//tracking
                    do {
                        detection_data = cap2prepare.receive();

                        det_image = detector.mat_to_image_resize(detection_data.cap_frame);
                        detection_data.det_image = det_image;
                        prepare2detect.send(detection_data);    // detection

#if 1//tracking
                        //가로 해상도 기준으로 리사이즈
                        tracking_data.exit_flag = detection_data.exit_flag;
                        //det_image_tracking = detector.mat_to_image_resize_byValue(detection_data.cap_frame, 500)
                        //tracking_data.det_image = det_image_tracking;
                        tracking_data.det_image = det_image;
                        prepare2tracking.send(tracking_data);
#endif
                        fps_prepare_counter++;
                    } while (!detection_data.exit_flag);
                    std::cout << " t_prepare exit \n";
                });


                // detection by Yolo
                if (t_detect.joinable()) t_detect.join();
                t_detect = std::thread([&]()
                {
                    std::shared_ptr<image_t> det_image;
                    detection_data_t detection_data;
                    do {
                        detection_data = prepare2detect.receive();
                        det_image = detection_data.det_image;
                        std::vector<bbox_t> result_vec;

                        if(det_image)
                            result_vec = detector.detect_resized(*det_image, frame_size.width, frame_size.height, thresh, true);  // true
                        fps_det_counter++;
                        //std::this_thread::sleep_for(std::chrono::milliseconds(150));

                        detection_data.new_detection = true;
                        detection_data.result_vec = result_vec;
                        detect2draw.send(detection_data);
                    } while (!detection_data.exit_flag);
                    std::cout << " t_detect exit \n";
                });

#if 1//tracking by opencv                
                if (t_tracking.joinable()) t_tracking.join();
                t_tracking = std::thread([&]()
                    {
                        std::shared_ptr<image_t> det_image;
                        detection_data_t tracking_data;                        
                   
                        do {
                          tracking_data = prepare2tracking.receive();
                            det_image = tracking_data.det_image;

                            std::vector<bbox_t> result_tracking_vec;

                            if (det_image)
                            {
                                cv::Mat tempMat = image_to_mat(*det_image);
                                                 
                                if (g_trackingOnOff == TRUE)
                                {
                                    result_tracking_vec = ProcessTracking(tempMat);

                                    //printf("tracking process time %d ms. \n", (int)((what_time_is_it_now() - time) * 1000));
                                }
                            }
                            fps_tracking_counter++;
                            //printf("tracking thread cpu = %d\n", GetCurrentProcessorNumber());

                            tracking_data.new_detection = true;
                            tracking_data.result_vec = result_tracking_vec;
                            tracking2draw.send(tracking_data);
                        } while (!tracking_data.exit_flag);
                        std::cout << " t_tracking exit \n";
                    });
            
#endif
                // draw rectangles (and track objects)
                t_draw = std::thread([&]()
                {
                    std::queue<cv::Mat> track_optflow_queue;
                    detection_data_t detection_data;
                    detection_data_t tracking_data;//tracking
                    do {

                        // for Video-file
                        if (detection_sync) {
                            detection_data = detect2draw.receive();
                        }
                        // for Video-camera
                        else
                        {
                            // get new Detection result if present
                            if (detect2draw.is_object_present()) {
                                cv::Mat old_cap_frame = detection_data.cap_frame;   // use old captured frame
                                detection_data = detect2draw.receive();
                                if (!old_cap_frame.empty())
                                    detection_data.cap_frame = old_cap_frame;
                            }
                            // get new Captured frame
                            else {
                                std::vector<bbox_t> old_result_vec = detection_data.result_vec; // use old detections
                                detection_data = cap2draw.receive();
                                detection_data.result_vec = old_result_vec;
                            }

                            // get new Tracking result if present
                            if (tracking2draw.is_object_present()) {
                                
                                tracking_data = tracking2draw.receive();
                            }
                            else//use old
                            {
                                
                            }
                        }

                        cv::Mat cap_frame = detection_data.cap_frame;
                        cv::Mat draw_frame = detection_data.cap_frame.clone();
                        std::vector<bbox_t> result_vec = detection_data.result_vec;
                        std::vector<bbox_t> result_tracking_vec = tracking_data.result_vec;

#ifdef TRACK_OPTFLOW
                        if (detection_data.new_detection) {
                            tracker_flow.update_tracking_flow(detection_data.cap_frame, detection_data.result_vec);
                            while (track_optflow_queue.size() > 0) {
                                draw_frame = track_optflow_queue.back();
                                result_vec = tracker_flow.tracking_flow(track_optflow_queue.front(), false);
                                track_optflow_queue.pop();
                            }
                        }
                        else {
                            track_optflow_queue.push(cap_frame);
                            result_vec = tracker_flow.tracking_flow(cap_frame, false);
                        }
                        detection_data.new_detection = true;    // to correct kalman filter
#endif //TRACK_OPTFLOW

                        // track ID by using kalman filter
                        if (use_kalman_filter) {
                            if (detection_data.new_detection) {
                                result_vec = track_kalman.correct(result_vec);
                            }
                            else {
                                result_vec = track_kalman.predict();
                            }
                        }
                        // track ID by using custom function
                        else {
                            int frame_story = std::max(5, current_fps_cap.load());
                            result_vec = detector.tracking_id(result_vec, true, frame_story, 40);
                        }

                        if (use_zed_camera && !detection_data.zed_cloud.empty()) {
                            result_vec = get_3d_coordinates(result_vec, detection_data.zed_cloud);
                        }

                        //small_preview.set(draw_frame, result_vec);
                        //large_preview.set(draw_frame, result_vec);
                        draw_boxes(draw_frame, result_vec, obj_names, result_tracking_vec, current_fps_det, current_fps_cap,
                            current_fps_tracking);

                        //printf("current_fps_prepare = %d \r\n", (int)current_fps_prepare);
                        //show_console_result(result_vec, obj_names, detection_data.frame_id);
                        //large_preview.draw(draw_frame);
                        //small_preview.draw(draw_frame, true);

                        detection_data.result_vec = result_vec;
                        detection_data.draw_frame = draw_frame;
                        draw2show.send(detection_data);
                        if (send_network) draw2net.send(detection_data);
                        if (output_video.isOpened()) draw2write.send(detection_data);
                    } while (!detection_data.exit_flag);
                    std::cout << " t_draw exit \n";
                });


                // write frame to videofile
                //비디오 파일로 저장
                t_write = std::thread([&]()
                {
                    if (output_video.isOpened()) {
                        detection_data_t detection_data;
                        cv::Mat output_frame;
                        do {
                            detection_data = draw2write.receive();
                            if(detection_data.draw_frame.channels() == 4) cv::cvtColor(detection_data.draw_frame, output_frame, CV_RGBA2RGB);
                            else output_frame = detection_data.draw_frame;
                            output_video << output_frame;
                        } while (!detection_data.exit_flag);
                        output_video.release();
                    }
                    std::cout << " t_write exit \n";
                });

                // send detection to the network
                //Json 형태로 Http Send
                if (isFile)
                {
                    ReplaceAll(filename, "\\", "\\\\");                          
                }
                t_network = std::thread([&]()
                {
                    if (send_network) {
                        detection_data_t detection_data;
                        do {
                            detection_data = draw2net.receive();

              
                            detector.send_json_http(detection_data.result_vec, obj_names, detection_data.frame_id, filename, 400000, jsonPort);

                        } while (!detection_data.exit_flag);
                    }
                    std::cout << " t_network exit \n";
                });

                // std::thread t_cap, t_prepare, t_detect, t_tracking/*tracking*/, t_post, t_draw, t_write, t_network;
                //INT32 Affinity;
                //int priority;
                //SetThreadAffinityMask(t_cap.native_handle(), 1);//
                SetThreadPriority(t_cap.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

                //SetThreadAffinityMask(t_prepare.native_handle(), 2);//2
                //SetThreadPriority(t_prepare.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

                //SetThreadAffinityMask(t_detect.native_handle(), 4);//3
                //SetThreadPriority(t_detect.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

                //SetThreadAffinityMask(t_tracking.native_handle(), 8);//4
                //SetThreadPriority(t_tracking.native_handle(), THREAD_PRIORITY_LOWEST);

                //SetThreadAffinityMask(t_draw.native_handle(), 16);//5
                //SetThreadPriority(t_draw.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

                //SetThreadAffinityMask(t_network.native_handle(), 32);//6
                //SetThreadPriority(t_network.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

                //priority = GetThreadPriority(t_cap.native_handle());

                // show detection
                detection_data_t detection_data;
                do {

                    steady_end = std::chrono::steady_clock::now();
                    float time_sec = std::chrono::duration<double>(steady_end - steady_start).count();
                    if (time_sec >= 1) {
                        current_fps_det = fps_det_counter.load() / time_sec;
                        current_fps_tracking = fps_tracking_counter.load() / time_sec;
                        current_fps_cap = fps_cap_counter.load() / time_sec;
                        current_fps_prepare = fps_prepare_counter.load() / time_sec;

                        steady_start = steady_end;
                        fps_det_counter = 0;
                        fps_tracking_counter = 0;
                        fps_cap_counter = 0;
                        fps_prepare_counter = 0;
                    }

                    detection_data = draw2show.receive();
                    cv::Mat draw_frame = detection_data.draw_frame;

                    //if (extrapolate_flag) {
                    //    cv::putText(draw_frame, "extrapolate", cv::Point2f(10, 40), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(50, 50, 0), 2);
                    //}

                    if(showVideo)
                        cv::imshow(windowName, draw_frame);
                    int key = cv::waitKey(3);    // 3 or 16ms
                    if (key == 'f') show_small_boxes = !show_small_boxes;
                    if (key == 'p') while (true) if (cv::waitKey(100) == 'p') break;
                    //if (key == 'e') extrapolate_flag = !extrapolate_flag;
                    if (key == 27) { exit_flag = true;}

                    //std::cout << " current_fps_det = " << current_fps_det << ", current_fps_cap = " << current_fps_cap << std::endl;
                } while (!detection_data.exit_flag);
                std::cout << " show detection exit \n";

                cv::destroyWindow(windowName);
                // wait for all threads
                if (t_cap.joinable()) t_cap.join();
                if (t_prepare.joinable()) t_prepare.join();
                if (t_detect.joinable()) t_detect.join();
                if (t_tracking.joinable()) t_tracking.join();//tracking
                if (t_post.joinable()) t_post.join();
                if (t_draw.joinable()) t_draw.join();
                if (t_write.joinable()) t_write.join();
                if (t_network.joinable()) t_network.join();

                break;

            }
            else if (file_ext == "txt") {    // list of image files
                std::ifstream file(filename);
                if (!file.is_open()) std::cout << "File not found! \n";
                else
                    for (std::string line; file >> line;) {
                        std::cout << line << std::endl;
                        cv::Mat mat_img = cv::imread(line);
                        std::vector<bbox_t> result_vec = detector.detect(mat_img);
                        show_console_result(result_vec, obj_names);
                        //draw_boxes(mat_img, result_vec, obj_names);
                        //cv::imwrite("res_" + line, mat_img);
                    }

            }
            else {    // image file
                // to achive high performance for multiple images do these 2 lines in another thread
                cv::Mat mat_img = cv::imread(filename);
                auto det_image = detector.mat_to_image_resize(mat_img);

                auto start = std::chrono::steady_clock::now();
                std::vector<bbox_t> result_vec = detector.detect_resized(*det_image, mat_img.size().width, mat_img.size().height);
                std::vector<bbox_t> result_tracking_vec;
                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double> spent = end - start;
                std::cout << " Time: " << spent.count() << " sec \n";

                //result_vec = detector.tracking_id(result_vec);    // comment it - if track_id is not required
                draw_boxes(mat_img, result_vec, obj_names, result_tracking_vec);

                if (showVideo)
                    cv::imshow(windowName, mat_img);
                show_console_result(result_vec, obj_names);
                cv::waitKey(0);
            }
#else   // OPENCV
            //std::vector<bbox_t> result_vec = detector.detect(filename);

            auto img = detector.load_image(filename);
            std::vector<bbox_t> result_vec = detector.detect(img);
            detector.free_image(img);
            show_console_result(result_vec, obj_names);
#endif  // OPENCV
        }
        catch (std::exception &e) { std::cerr << "exception: " << e.what() << "\n"; getchar(); }
        catch (...) { std::cerr << "unknown exception \n"; getchar(); }
        filename.clear();
    }

    return 0;
}

