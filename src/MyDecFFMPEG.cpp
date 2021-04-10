
#include "MyDecFFMPEG.h"
#include <thread>
#include "opencv2/core/ocl.hpp"

string g_strWindowName;
void WriteLog(const char * message)
{	
#if false//로그필요시 true 설정
	if (g_strWindowName != "")
	{
		printf("%s \r\n", message);
		FILE * file = fopen(g_strWindowName + "_log.txt", "a+");

		int number = 1;

		struct tm *t;
		time_t timer;
		timer = time(NULL);    // 현재 시각을 초 단위로 얻기
		t = localtime(&timer); // 초 단위의 시간을 분리하여 구조체에 넣기
		fprintf(file, "[%04d-%02d-%02d %02d:%02d:%02d] ",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec
		);

		fprintf(file, "%s \r\n", message);
		fclose(file);
	}
#endif
}


CMyDecFFMPEG::CMyDecFFMPEG()
{
	m_callBackFunc = NULL;
	input_ctx = NULL;
	m_bSaveJpeg = false;
	m_bPanoCap = false;
	m_bRecord = false;
	m_bCaptureWithOSD = false;
	m_nMainCh = 0;
	m_nSubCh = 0;
	m_strJpegFileName = "temp";
	m_strRecFileName = "temp";
	m_recStatus = REC_STATUS::REC_IDLE;
	m_capStatus = CAP_STATUS::CAP_IDLE;

	m_bVideoThreadRun = false;
	m_bVideoThreadQuit = false;
	m_fpsctrl.SetFPS(10);
	m_fpsctrlForBlurDetec.SetFPS(15);

	memset(m_bArrDefog, 0x00, sizeof(m_bArrDefog));
    memset(m_bArrStab, 0x00, sizeof(m_bArrStab));
    memset(m_nArrSwDigitalZoom, 0x00, sizeof(m_nArrSwDigitalZoom));
	
	

	
}


CMyDecFFMPEG::~CMyDecFFMPEG()
{
}

UINT CMyDecFFMPEG::VideoReadThread(LPVOID lParam)
{
	//WriteLog("start VideoReadThread");
	CMyDecFFMPEG * ptr = (CMyDecFFMPEG*)lParam;

	//종료 플래그 설정
	ptr->m_bVideoThreadQuit = false;
	ptr->m_bVideoThreadRun = true;

	/* actual decoding and dump the raw data */
	int ret = 0;
	while (ret >= 0 && ptr && !ptr->m_bVideoThreadQuit) {		
		if ((ret = av_read_frame(ptr->input_ctx, &ptr->packet)) < 0)
			break;
		
		if (ptr->video_stream == ptr->packet.stream_index)
		{
			ret = ptr->decode_write(ptr->decoder_ctx, &ptr->packet);
			if (ret < 0)
				ret = 0;//디코딩 에러는 무시 FujiFilm SX800 카메라 초반에 디코딩 에러남
		}

		av_packet_unref(&ptr->packet);
	}

	if (ptr->m_callBackFunc != NULL)
		ptr->m_callBackFunc(NULL, "[VideoReadThread] return = 0");

	/* flush the decoder */
	ptr->packet.data = NULL;
	ptr->packet.size = 0;
	ret = ptr->decode_write(ptr->decoder_ctx, &ptr->packet);
	av_packet_unref(&ptr->packet);


	avcodec_free_context(&ptr->decoder_ctx);
	avformat_close_input(&ptr->input_ctx);

	if (ptr->m_nHwDeviceID != 0)
		av_buffer_unref(&ptr->hw_device_ctx);

	sws_freeContext(ptr->m_swsctxToBGR24);
	sws_freeContext(ptr->m_swsctxToYUV420);

	av_freep(&ptr->m_avFrameYUV420->data[0]);
	av_frame_free(&ptr->m_avFrameYUV420);


	ptr->m_bVideoThreadRun = false;
	TRACE("return VideoReadThread \r\n");

	//WriteLog("return VideoReadThread");
	return 0;
}
int CMyDecFFMPEG::decode_write(AVCodecContext * avctx, AVPacket * packet)
{
	AVFrame *frame = NULL, *sw_frame = NULL;
	AVFrame *tmp_frame = NULL;
	uint8_t *buffer = NULL;
	int size;
	int ret = 0;

	ret = avcodec_send_packet(avctx, packet);
	if (ret < 0) {
		fprintf(stderr, "Error during decoding\n");
		return ret;
	}

	while (1) {
		if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
			fprintf(stderr, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		ret = avcodec_receive_frame(avctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			return ret;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error while decoding\n");
			goto fail;
		}


		//if HW 디코딩
		if (frame->format == hw_pix_fmt && m_nHwDeviceID != 0) {
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
				fprintf(stderr, "Error transferring the data to system memory\n");
				goto fail;
			}
			tmp_frame = sw_frame;
		}
		else
			tmp_frame = frame;

		size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format, tmp_frame->width,
			tmp_frame->height, 1);
		buffer = (uint8_t*)av_malloc(size);
		if (!buffer) {
			fprintf(stderr, "Can not alloc buffer\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = av_image_copy_to_buffer(buffer, size,
			(const uint8_t * const *)tmp_frame->data,
			(const int *)tmp_frame->linesize, (AVPixelFormat)tmp_frame->format,
			tmp_frame->width, tmp_frame->height, 1);
		if (ret < 0) {
			fprintf(stderr, "Can not copy image to buffer\n");
			goto fail;
		}	

		{
		

#if 0
			int width = tmp_frame->width, height = tmp_frame->height;
			cv::Mat tmp_img = cv::Mat::zeros(height * 3 / 2, width, CV_8UC1);
			memcpy(tmp_img.data, tmp_frame->data[0], width*height);
			memcpy(tmp_img.data + width * height, tmp_frame->data[1], width*height / 4);
			memcpy(tmp_img.data + width * height * 5 / 4, tmp_frame->data[2], width*height / 4);

			//cv::imshow("yuv_show", tmp_img);
			cv::UMat tempUmat = tmp_img.getUMat(cv::ACCESS_READ);

			//cv::Mat bgr;
			cv::UMat ubgr;

			cv::cvtColor(tempUmat, ubgr, CV_YUV2BGR_I420);
			cv::Mat srcMat;
			ubgr.getMat(cv::ACCESS_READ).copyTo(srcMat);

			
#endif
			
			//imwrite("test.jpg", srcMat);
			//cv::Mat srcMat;
			//return 0;

			

			
			if (m_bSaveJpeg || m_bPanoCap || m_bRecord || m_bAf || m_bArrDefog[m_nMainCh - 1] || m_bArrStab[m_nMainCh - 1] ||
				(m_nArrSwDigitalZoom[m_nMainCh - 1] > 1))
			{
				cv::Mat srcMat = avframe_to_cvmat(tmp_frame);

				if (m_bArrDefog[m_nMainCh - 1] || m_bArrStab[m_nMainCh - 1] || (m_nArrSwDigitalZoom[m_nMainCh - 1] > 1))
				{
					m_spectacles.Spectacles(srcMat);

					if (m_nArrSwDigitalZoom[m_nMainCh - 1] > 1)
					{
						int mag = m_nArrSwDigitalZoom[m_nMainCh - 1];
						IplImage * srcImg = new IplImage(srcMat);
						IplImage * resizedImg = m_spectacles.ResizeImg(srcImg, srcMat.cols * mag, srcMat.rows * mag);
						srcMat = cvarrToMat(resizedImg);
						// 관심영역 설정 (set ROI (X, Y, W, H)).
						Rect rect((srcMat.cols / 2) - (tmp_frame->width / 2), (srcMat.rows / 2) - (tmp_frame->height / 2),
							tmp_frame->width, tmp_frame->height);
						// 관심영역 자르기 (Crop ROI).
						srcMat = srcMat(rect);
						delete srcImg;
					}
			
					cvmatToAvframe(&srcMat);
				}			

				if (m_bAf)
					m_pSwAF->SetFrame(srcMat, m_nCurFocus);
				//SaveFrame
				if (m_bSaveJpeg)
				{
					if (MatToJpeg(srcMat, m_strJpegFileName))
						m_capStatus = CAP_STATUS::CAP_SUCCESS;
					else
						m_capStatus = CAP_STATUS::CAP_FAILURE;


					m_bSaveJpeg = false;
					m_bPanoCap = false;

					/*	av_freep(&pByDstBufBGR);
						av_frame_free(&avFrameBGR);*/

				}

				if (m_bPanoCap)
				{
					/*AVFrame * _avFrameBGR = NULL;
					uint8_t * _pByDstBufBGR = NULL;*/

					int destWidth = 384;//480, 384
					int destHeight = 216;//270, 216
					//int destWidth = 480;//480, 384
					//int destHeight = 270;//270, 216
					//AVFrametoBGR24(tmp_frame, &avFrameBGR, &pByDstBufBGR, destWidth, destHeight);
					////int cv_format = CV_8UC3;
					//cv::Mat mat(destHeight, destWidth, cv_format, avFrameBGR->data[0], avFrameBGR->linesize[0]);

					cv::Mat DstMat;
					cv::resize(srcMat, DstMat, cv::Size(destWidth, destHeight));
					if (MatToPanoJpeg(DstMat, m_strPanoCapName))
					{
						m_capPathNameStatus = CAP_STATUS::CAP_SUCCESS;
						m_strPanoCapName = "";
					}
					else
					{
						m_capPathNameStatus = CAP_STATUS::CAP_FAILURE;
						m_strPanoCapName = "";
						WriteLog(m_strPanoCapName.c_str());
					}

					m_bSaveJpeg = false;
					m_bPanoCap = false;

					/*	av_freep(&_pByDstBufBGR);
						av_frame_free(&_avFrameBGR);*/
				}

				if (m_bRecord)
				{
					if (m_fpsctrl.CheckRecordTiming())//루프를 돌때 1프레임을 저장할 타이밍을 알려준다.
					{
						m_fpsctrl.addFrame();

						/*AVFrame * _avFrameBGR = NULL;
						uint8_t * _pByDstBufBGR = NULL;*/


						//원본소스가 1920 1080 이면
						int destWidth = tmp_frame->width;
						int destHeight = tmp_frame->height;
						if (destWidth == 1920 && destHeight == 1080)
						{
							destWidth /= 2;
							destHeight /= 2;
						}
						//AVFrametoBGR24(tmp_frame, &avFrameBGR, &pByDstBufBGR, destWidth, destHeight);
						//int cv_format = CV_8UC3;
						//cv::Mat mat(avFrameBGR->height, avFrameBGR->width, cv_format, avFrameBGR->data[0], avFrameBGR->linesize[0]);

						cv::Mat DstMat;
						cv::resize(srcMat, DstMat, cv::Size(destWidth, destHeight));

						MatToVideoFile(DstMat, m_strRecFileName, destWidth, destHeight, m_fpsctrl.GetFPS());

						/*	av_freep(&_pByDstBufBGR);
							av_frame_free(&_avFrameBGR);*/
					}
				}
				else
				{
					CloseVideoFile();
				}
			}
			else
			{
				CloseVideoFile();
			}

			if (m_callBackFunc != NULL)
			{
				if (m_bArrDefog[m_nMainCh - 1] || m_bArrStab[m_nMainCh - 1] || (m_nArrSwDigitalZoom[m_nMainCh - 1] > 1))
				{
					m_callBackFunc(m_avFrameYUV420, "decode_write() success");
				}
				else
					m_callBackFunc(tmp_frame, "decode_write() success");
			}
		
			
		}
	
		/*av_freep(&pByDstBufBGR);
		av_frame_free(&avFrameBGR);*/
	fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
		av_freep(&buffer);
		if (ret < 0)
			return ret;
	}

}

bool CMyDecFFMPEG::MatToJpeg(cv::Mat &mat, string fileName)
{
	string timeName = GetTimeFileName();
	string name = m_strCapDirPath + fileName + "_" + timeName + ".jpg";
	return cv::imwrite(name, mat);
}
bool CMyDecFFMPEG::MatToPanoJpeg(cv::Mat &mat, string fileName)
{
	string name = m_strPanoCapDirPath + fileName + ".jpg";
	m_strPanoCapName = name;
	return cv::imwrite(name, mat);
}


int CMyDecFFMPEG::InitMyDec(string strRtspUrl, int nHwDeviceID, fnCallBackFunc funcPtr, int & videoSrcWidth, int & videoSrcHeight)
{
	//WriteLog("start InitMyDec()");

	m_callBackFunc = funcPtr;
	m_nHwDeviceID = nHwDeviceID;
	if (nHwDeviceID == 0)//CPU Decode
	{
		/*	AVFormatContext *pFormatCtx = avformat_alloc_context();*/
		int             i;/* , videoStream;*/
		/*AVCodecContext  *pCodecCtx;*/
		AVCodec         *pCodec;

		AVFrame         *pFrameRGB;
		AVPacket        packet;
		int             frameFinished;
		int             numBytes;
		uint8_t         *buffer;

		/* open the input file */
		AVDictionary *stream_opts = 0;
		av_dict_set(&stream_opts, "rtsp_transport", "tcp", 0);//한프로그램에서 스레드 여러개 돌리면 속도가 느려짐?
															  //av_dict_set(&stream_opts, "rtsp_transport", "http", 0);
															  //av_dict_set(&stream_opts, "rtsp_flags", "prefer_tcp", 0);
															  //av_dict_set(&stream_opts, "rtsp_flags", "listen", 0);
															  //av_dict_set(&stream_opts, "reorder_queue_size", "tcp", 0);

		av_dict_set(&stream_opts, "stimeout", "10000000", 0); // in microseconds.
															  //av_dict_set(&stream_opts, "max_alloc ", "10000", 0);

		if (avformat_open_input(&this->input_ctx, strRtspUrl.c_str(), NULL, &stream_opts) != 0) {
			fprintf(stderr, "Cannot open input file '%s'\n", strRtspUrl.c_str());
			return -1;
		}

		if (avformat_find_stream_info(this->input_ctx, NULL) < 0) {
			fprintf(stderr, "Cannot find input stream information.\n");
			return -1;
		}

		/* find the video stream information */
		int ret = av_find_best_stream(this->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
		if (ret < 0) {
			fprintf(stderr, "Cannot find a video stream in the input file\n");
			return -1;
		}
		this->video_stream = ret;

		// Get a pointer to the codec context for the video stream
		//
		if (!(this->decoder_ctx = avcodec_alloc_context3(pCodec)))
		{
			return AVERROR(ENOMEM);
		}
		AVStream *video = NULL;
		video = this->input_ctx->streams[this->video_stream];
		if (avcodec_parameters_to_context(this->decoder_ctx, video->codecpar) < 0)
		{
			return -1;
		}
		//


		// Find the decoder for the video stream
		pCodec = avcodec_find_decoder(this->decoder_ctx->codec_id);
		if (pCodec == NULL) {
			fprintf(stderr, "Unsupported codec!\n");
			return -1; // Codec not found
		}
		// Open codec
		//if(avcodec_open(pCodecCtx, pCodec)<0)
		if (avcodec_open2(this->decoder_ctx, pCodec, NULL) < 0)
			return -1; // Could not open codec

	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////HW Decode//////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	else
	{
		int ret;
		AVStream *video = NULL;
		AVCodec *decoder = NULL;
		enum AVHWDeviceType type;


		//if (argc < 4) {
		//	fprintf(stderr, "Usage: %s <device type> <input file> <output file>\n", argv[0]);
		//	return -1;
		//}
		int i;
		if (nHwDeviceID != 0)
			i = nHwDeviceID;
		const int MAX_HWDEVICE = 2;
		for (; i <= MAX_HWDEVICE; i++)
		{
			string strHWDeviceName = "";
			switch (i)
			{
			case 1:
				strHWDeviceName = "cuda";
				break;
			case 2:
				strHWDeviceName = "dxva2";
				break;
			default:
				return -1;
			}

			type = av_hwdevice_find_type_by_name(strHWDeviceName.c_str());
			if (type == AV_HWDEVICE_TYPE_NONE) {
				fprintf(stderr, "Device type %s is not supported.\n", strHWDeviceName);
				fprintf(stderr, "Available device types:");
				while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
					fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
				fprintf(stderr, "\n");
				//return -1;
				continue;
			}
			break;//av_hwdevice_find_type_by_name() 성공
		}

		/* open the input file */
		AVDictionary *stream_opts = 0;
		av_dict_set(&stream_opts, "rtsp_transport", "tcp", 0);
		//av_dict_set(&stream_opts, "rtsp_transport", "http", 0);
		//av_dict_set(&stream_opts, "rtsp_flags", "prefer_tcp", 0);
		//av_dict_set(&stream_opts, "rtsp_flags", "listen", 0);
		//av_dict_set(&stream_opts, "reorder_queue_size", "tcp", 0);

		av_dict_set(&stream_opts, "stimeout", "10000000", 0); // in microseconds.
															  //av_dict_set(&stream_opts, "max_alloc ", "10000", 0);

		//rtsp://admin:admin1234@@192.168.0.64:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1
		//rtsp://admin:q1w2e3r4t5@192.168.0.100:554/profile2/media.smp
		if (avformat_open_input(&this->input_ctx, strRtspUrl.c_str(), NULL, &stream_opts) != 0) {
			fprintf(stderr, "Cannot open input file '%s'\n", strRtspUrl);
			return -1;
		}

		if (avformat_find_stream_info(input_ctx, NULL) < 0) {
			fprintf(stderr, "Cannot find input stream information.\n");
			return -1;
		}

		/* find the video stream information */
		ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
		if (ret < 0) {
			fprintf(stderr, "Cannot find a video stream in the input file\n");
			return -1;
		}
		this->video_stream = ret;

		for (i = 0;; i++) {
			const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
			if (!config) {
				fprintf(stderr, "Decoder %s does not support device type %s.\n",
					decoder->name, av_hwdevice_get_type_name(type));
				return -1;
			}
			if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
				config->device_type == type) {
				hw_pix_fmt = config->pix_fmt;
				break;
			}
		}

		if (!(this->decoder_ctx = avcodec_alloc_context3(decoder)))
			return AVERROR(ENOMEM);

		video = input_ctx->streams[this->video_stream];
		if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
			return -1;

		this->decoder_ctx->get_format = CMyDecFFMPEG::get_hw_format;

		if (hw_decoder_init(this->decoder_ctx, type) < 0)
			return -1;

		if ((ret = avcodec_open2(this->decoder_ctx, decoder, NULL)) < 0) {
			fprintf(stderr, "Failed to open codec for stream #%u\n", this->video_stream);
			return -1;
		}
	}



	enum AVPixelFormat src_pixfmt = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat dst_pixfmt = AV_PIX_FMT_BGR24;

	m_swsctxToBGR24 = sws_getContext(this->decoder_ctx->width, this->decoder_ctx->height, src_pixfmt,
		this->decoder_ctx->width, this->decoder_ctx->height, dst_pixfmt,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);

	m_swsctxToYUV420 = sws_getContext(this->decoder_ctx->width, this->decoder_ctx->height, dst_pixfmt,
		this->decoder_ctx->width, this->decoder_ctx->height, src_pixfmt,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);

	m_avFrameYUV420 = av_frame_alloc();
	av_image_alloc(m_avFrameYUV420->data, m_avFrameYUV420->linesize, this->decoder_ctx->width, this->decoder_ctx->height, AVPixelFormat::AV_PIX_FMT_YUV420P, 1);

	
#if 0//간헐적으로 this->decoder_ctx->width and height 0으로 나옴
	videoSrcWidth = this->decoder_ctx->width;
	videoSrcHeight = this->decoder_ctx->height;
	CString str;
	str.Format(_T("Success InitMyDec() %dx%d"), videoSrcWidth, videoSrcHeight);
	WriteLog((CStringA)str.GetBuffer());
#endif
	//WriteLog("finish InitMyDec()");
	//StartThread
	AfxBeginThread(VideoReadThread, this, THREAD_PRIORITY_NORMAL);
	return TRUE;
}

void CMyDecFFMPEG::DisableCallback()
{
	m_callBackFunc = NULL;
}

void CMyDecFFMPEG::ActivateCallback(fnCallBackFunc fptr)
{
	m_callBackFunc = fptr;
}
void CMyDecFFMPEG::SetAfCallBack(fnAfCallBackFunc fptr)
{
	m_pSwAF = new CSWAutoFocus(fptr);
}
string CMyDecFFMPEG::GetTimeFileName()
{
	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	static char result[26];
	sprintf(result, "%d_%02d_%02d_%02d_%02d_%02d",
		1900 + timeinfo->tm_year,
		timeinfo->tm_mon + 1,
		timeinfo->tm_mday, timeinfo->tm_hour,
		timeinfo->tm_min, timeinfo->tm_sec
	);

	std::ostringstream stringStream;
	stringStream << result;
	return stringStream.str();

}

REC_STATUS CMyDecFFMPEG::GetRecStatus() {
	return m_recStatus;
}
CAP_STATUS CMyDecFFMPEG::GetCaptureStatus()
{
	return m_capStatus;
}
CAP_STATUS CMyDecFFMPEG::GetCapturePathNameStatus()
{
	return m_capPathNameStatus;
}
void CMyDecFFMPEG::ResetCaptureStatus()
{
	m_capStatus = CAP_STATUS::CAP_IDLE;
}
void CMyDecFFMPEG::ResetCapturePathNameStatus()
{
	m_capPathNameStatus = CAP_STATUS::CAP_IDLE;	
}
void CMyDecFFMPEG::AVFrametoBGR24(AVFrame * frameSrc, AVFrame ** avFrameDstBGR, uint8_t ** pByDstBufBGR, int destWidth, int destHeight)
{
	if (destWidth == 0 || destHeight == 0)
	{
		destWidth = frameSrc->width;
		destHeight = frameSrc->height;
	}
	SwsContext * pSwsCtx = sws_getContext(
		frameSrc->width,
		frameSrc->height,
		(AVPixelFormat)frameSrc->format,
		destWidth,
		destHeight,
		AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);

	AVFrame * avFrameBGR = av_frame_alloc();
	int nBufSizeRendering = avpicture_get_size(AV_PIX_FMT_BGR24, destWidth, destHeight);
	uint8_t * pByBufBGR = (uint8_t *)av_malloc(nBufSizeRendering);
	avpicture_fill((AVPicture *)avFrameBGR, pByBufBGR, AV_PIX_FMT_BGR24, destWidth, destHeight);//AVFrame, pByBuf 연결	

	sws_scale(pSwsCtx, ((AVPicture*)frameSrc)->data, ((AVPicture*)frameSrc)->linesize,
		0, frameSrc->height, ((AVPicture*)avFrameBGR)->data, avFrameBGR->linesize);
	avFrameBGR->width = destWidth;
	avFrameBGR->height = destHeight;

	sws_freeContext(pSwsCtx);
	//av_free(pByBufBGR);

	*pByDstBufBGR = pByBufBGR;
	*avFrameDstBGR = avFrameBGR;
}

cv::Mat CMyDecFFMPEG::avframe_to_cvmat(AVFrame *frame)
{
#if 1
	
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
#else
	

	int width = frame->width, height = frame->height;
	cv::Mat tmp_img = cv::Mat::zeros(height * 3 / 2, width, CV_8UC1);
	memcpy(tmp_img.data, frame->data[0], width*height);
	memcpy(tmp_img.data + width * height, frame->data[1], width*height / 4);
	memcpy(tmp_img.data + width * height * 5 / 4, frame->data[2], width*height / 4);

	//cv::imshow("yuv_show", tmp_img);
	cv::UMat tempUmat = tmp_img.getUMat(cv::ACCESS_READ);

	//cv::Mat bgr;
	cv::UMat ubgr;

	cv::cvtColor(tempUmat, ubgr, CV_YUV2BGR_I420);
	cv::Mat bgr = ubgr.getMat(cv::ACCESS_READ);
	imshow("windows", bgr);
	return bgr;
#endif

}
//cv::Mat 转 AVFrame  
void CMyDecFFMPEG::cvmatToAvframe(cv::Mat* image) {
	int width = image->cols;
	int height = image->rows;
	int cvLinesizes[1];
	cvLinesizes[0] = image->step1();
	//if (frame == NULL) {
	//	frame = av_frame_alloc();
	//	av_image_alloc(frame->data, frame->linesize, width, height, AVPixelFormat::AV_PIX_FMT_YUV420P, 1);
	//}
	
	sws_scale(m_swsctxToYUV420, &image->data, cvLinesizes, 0, height, m_avFrameYUV420->data, m_avFrameYUV420->linesize);
	

	m_avFrameYUV420->width = width;
	m_avFrameYUV420->height = height;
	
}
