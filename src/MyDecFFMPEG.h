#pragma once



extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include "libswscale/swscale.h"

#pragma comment ( lib, "avformat.lib")
#pragma comment ( lib, "swscale.lib" )
#pragma comment ( lib, "avcodec.lib" )
#pragma comment ( lib, "avutil.lib" )
#pragma comment ( lib, "avdevice.lib" )
}


using namespace std;
#include <iostream>

static enum AVPixelFormat hw_pix_fmt;


typedef void (CALLBACK * fnCallBackFunc)(AVFrame *frame, string result);

enum CAP_STATUS
{
	CAP_IDLE = 1,
	CAP_SUCCESS,
	CAP_FAILURE
};

enum REC_STATUS
{
	REC_IDLE = 1,
	REC_RECORDING
};

class CMyDecFFMPEG
{
public:
	CMyDecFFMPEG();
	~CMyDecFFMPEG();
private:
	AVBufferRef *hw_device_ctx;
	AVFormatContext *input_ctx;
	AVPacket packet;
	int video_stream;
	AVCodecContext *decoder_ctx;

	//Capture
	bool m_bSaveJpeg;
	string m_strJpegFileName;
	bool m_bCaptureWithOSD;
	CAP_STATUS m_capStatus;	
	//
	//PanoCap
	bool m_bPanoCap;
	string m_strPanoCapName;
	CAP_STATUS m_capPathNameStatus;
	//
	//Record
	bool m_bRecord;
	string m_strRecFileName;
	REC_STATUS m_recStatus;
	//
	cv::VideoWriter videoWriter;
	string m_strRecDirPath;
	string m_strCapDirPath;
	string m_strPanoCapDirPath;
	
	int m_nMainCh;
	int m_nSubCh;
	bool m_bVideoThreadRun;
	bool m_bVideoThreadQuit;
	int m_nHwDeviceID;

	bool m_bArrDefog[10];
	bool m_bArrStab[10];
	int m_nArrSwDigitalZoom[10];
	
	int m_bAf = false;
	int m_nCurFocus;

	SwsContext * m_swsctxToBGR24 = NULL;
	SwsContext * m_swsctxToYUV420 = NULL;
	AVFrame * m_avFrameYUV420 = NULL;
public:
	//클래스 초기화 함수
	//HW Device 선택
	//int nHwDeviceID
	//0. CPU
	//1. CUVID
	//2. DXVA
	int InitMyDec(string strRtspUrl, int nHwDeviceID, fnCallBackFunc funcPtr, int & videoSrcWidth, int & videoSrcHeight);
	void DisableCallback();
	void ActivateCallback(fnCallBackFunc fptr);
	void SetAfCallBack(fnAfCallBackFunc fptr);
	static UINT VideoReadThread(LPVOID lParam);
	inline int decode_write(AVCodecContext *avctx, AVPacket *packet);

	bool MatToJpeg(cv::Mat& mat, string fileName);
	bool MatToPanoJpeg(cv::Mat &mat, string fileName);
	bool MatToVideoFile(cv::Mat& mat, string fileName, int width, int height, int fps);
	void CloseVideoFile();
	void SetChInfo(int nMainCh, int nSubCh);	

	//스레드 상태
	bool GetVideoThreadRun() {
		return m_bVideoThreadRun;
	}
	//비디오스레드 종료 플래그
	void SetVideoThreadQuit() {
		m_bVideoThreadQuit = true;
	}
	//Capture
	void StartCaptureWithOSD(string fileName, bool withOSD){
		m_bSaveJpeg = true;
		m_strJpegFileName = fileName;
		m_bCaptureWithOSD = withOSD;
	}
	void StartPanoCapture(string fileName){
		m_strPanoCapName = fileName; 
		m_bPanoCap = true;		
	}
	//Record
	void StartRecord(string fileName){
		m_bRecord = true;
		m_strRecFileName = fileName;
	}
	void StopRecord(){
		m_bRecord = false;
	}
	CString GetRecStrStatus();
	REC_STATUS GetRecStatus();
	CAP_STATUS GetCaptureStatus();
	CAP_STATUS GetCapturePathNameStatus();
	string GetPanoCapturePathName() {
		return m_strPanoCapName;
	}
	void ResetCaptureStatus();
	void ResetCapturePathNameStatus();

	void SetDefogMode(bool on)
	{
		m_bArrDefog[m_nMainCh - 1] = on;
		m_spectacles.SetDeFogMode(on);
	}	
	void SetSwDigitalZoom(int dZoom)
	{
		m_nArrSwDigitalZoom[m_nMainCh - 1] = dZoom;
	}
	void SetImgStabMode(bool on)
	{
		m_bArrStab[m_nMainCh - 1] = on;
		m_spectacles.SetImgStabMode(on);
	}
	void SetDefogMode()
	{
		m_spectacles.SetDeFogMode(m_bArrDefog[m_nMainCh - 1]);
	}
	void SetImgStabMode()
	{
		m_spectacles.SetImgStabMode(m_bArrStab[m_nMainCh - 1]);
	}
	void StartAF(bool start)
	{
		m_bAf = start;
		if (start)
		{
			m_pSwAF->StartAF();
		}
		else
		{
			m_pSwAF->FinishAF();
		}
	}
	void SetCurFocus(int val)
	{
		if(val >= 0 && val <= 1000)
			m_nCurFocus = val;
	}
private:
	
	void AVFrametoBGR24(AVFrame * frameSrc, AVFrame ** avFrameDstBGR, uint8_t ** pByDstBufBGR, int destWidth=0, int destHeight = 0);
	cv::Mat avframe_to_cvmat(AVFrame * frame);
	void cvmatToAvframe(cv::Mat * image);

	string GetTimeFileName();
	fnCallBackFunc m_callBackFunc;
	int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
	{
		int err = 0;

		if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
			NULL, NULL, 0)) < 0) {
			fprintf(stderr, "Failed to create specified HW device.\n");
			return err;
		}
		ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

		return err;
	}
	static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
		const enum AVPixelFormat *pix_fmts)
	{
		const enum AVPixelFormat *p;

		for (p = pix_fmts; *p != -1; p++) {
			if (*p == hw_pix_fmt)
				return *p;
		}

		fprintf(stderr, "Failed to get HW surface format.\n");
		return AV_PIX_FMT_NONE;
	}
};

