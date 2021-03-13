#pragma once


#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <queue>
using namespace std;

class CTrackingInterfaceUdpSocket
{

#if 1//공통
public:
	BOOL CreateSocket(
		string strDestAddr,//Destination 주소 그룹 주소,
		int destPort,//Destination Port
		string strHostAddr,//Host 주소
		int hostPort,//패킷을 수신 할 멀티 캐스트 그룹의 포트
		string socketName//소켓 이름(디버그 출력용)
	);
private:
	std::string m_socketName;
#endif


#if 1//Sender
private:
	SOCKET m_senderSock;
	std::string m_strDestAddr;
	int m_nDestPort;
	std::queue<std::string> m_senderQueue;
	//CRITICAL_SECTION m_cs;
public:
	void SetSendData(std::string strData) {
		//EnterCriticalSection(&m_cs);
		m_senderQueue.push(strData);
		//LeaveCriticalSection(&m_cs);
	}
	std::string PopSendData()
	{
		//EnterCriticalSection(&m_cs);
		std::string strData = m_senderQueue.front();
		m_senderQueue.pop();
		//LeaveCriticalSection(&m_cs);
		return strData;
	}
	SOCKET GetSenderSocket() {
		return m_senderSock;
	}
	std::string GetDestAddr() {
		return m_strDestAddr;
	}
	int GetDestPort() {
		return m_nDestPort;
	}
#endif

#if 1//Receiver
private:
	SOCKET m_recverSock;
	std::string m_strRecvAddr;
	int m_nRecvPort;
	std::queue<std::string> m_recverQueue;
	//CRITICAL_SECTION m_cs;
public:
	void SetRecvData(std::string strData) {
		//EnterCriticalSection(&m_cs);
		m_recverQueue.push(strData);
		//LeaveCriticalSection(&m_cs);
	}
	std::string PopRecvData()
	{
		//EnterCriticalSection(&m_cs);
		std::string strData = m_recverQueue.front();
		m_recverQueue.pop();
		//LeaveCriticalSection(&m_cs);
		return strData;
	}
	SOCKET GetRecverSocket() {
		return m_recverSock;
	}
	std::string GetRecvAddr() {
		return m_strRecvAddr;
	}
	int GetRecvPort() {
		return m_nRecvPort;
	}
#endif

};

