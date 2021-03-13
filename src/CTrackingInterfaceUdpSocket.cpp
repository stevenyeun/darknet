#include "CTrackingInterfaceUdpSocket.h"

BOOL CTrackingInterfaceUdpSocket::CreateSocket(string strDestAddr, int destPort, string strHostAddr, int hostPort, string socketName)
{
	m_socketName = socketName;
	int err;

#if 1//Sender	
	m_strDestAddr = strDestAddr;
	m_nDestPort = destPort;
	int ttlValue = 1;					// TTL
	

	WSADATA WSAData_Send;
	err = WSAStartup(MAKEWORD(2, 2), &WSAData_Send);
	if (err)
	{
		printf("WSAStartup Failed (%d) Exiting\n", err);
		return FALSE;
	}


	// Create a normal UDP socket for sending data
	m_senderSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_senderSock == INVALID_SOCKET)
	{
		err = GetLastError();
		printf("\"Socket\" failed with error %d\n", err);
		return FALSE;
	}

	//sockaddr_in send_addr;
	//memset(&send_addr, 0, sizeof(SOCKADDR_IN));
	//send_addr.sin_family = AF_INET;
	//send_addr.sin_addr.s_addr = inet_addr(strDestAddr.c_str());					// this is our test destination IP
	//send_addr.sin_port = htons(destPort);

	


	// Set the Time-to-Live of the multicast
	/*if (setsockopt(m_senderSock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttlValue, sizeof(int)) == SOCKET_ERROR)
	{
		TRACE("Error: Setting TTL value: error - %d\n", WSAGetLastError());
		closesocket(m_sock);
		return FALSE;
	}*/



	//스레드 생성
	//AfxBeginThread(SenderThread, this, THREAD_PRIORITY_NORMAL);
#endif


#if 1//Recver
	

	//int ttlValue = 1;					// TTL
	WSADATA WSAData_Recv;

	err = WSAStartup(MAKEWORD(2, 2), &WSAData_Recv);
	if (err)
	{
		//TRACE("WSAStartup Failed (%d) Exiting\n", err);
		exit(err);
	}
	// Create a normal UDP socket for sending data
	m_recverSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_recverSock == INVALID_SOCKET)
	{
		err = GetLastError();
		//TRACE("\"Socket\" failed with error %d\n", err);
		return FALSE;
	}

	//Steven180508
	//주소 재사용해서 바인딩 하기(같은 호스트에서 포트 중복 허용)	
	int opt = 1;//true
	if (setsockopt(m_recverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == -1)
	{
		//TRACE("\nError: Joining Multicast Group - %d", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}
	//
	sockaddr_in receive_addr;		// Local socket's address. Holds the source address upon recvfrom function returns
	receive_addr.sin_family = AF_INET;
	//receive_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	receive_addr.sin_addr.s_addr = inet_addr(strHostAddr.c_str());
	receive_addr.sin_port = htons(destPort);
	int nRecvLen = sizeof(receive_addr);

	// Binding the socket to the address. Only necessary on receiver
	if (bind(m_recverSock, (sockaddr*)&receive_addr, sizeof(receive_addr)) == SOCKET_ERROR)
	{
		//TRACE("\nError: Binding the socket - %d", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}
	//TRACE("\nSuccess: Binding the socket - %d", WSAGetLastError());


	//ip_mreq req;
	//req.imr_multiaddr.s_addr = inet_addr((CStringA)strDestAddr);
	//req.imr_interface.s_addr = INADDR_ANY;
	//// Joining a multicast group
	//if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&req, sizeof(req)) == -1)
	//{
	//	TRACE("\nError: Joining Multicast Group - %d", WSAGetLastError());
	//	WSACleanup();
	//	return FALSE;
	//}


	//TRACE("\nSuccess: Joining Multicast Group - %d\n", WSAGetLastError());

	//스레드 생성
	//m_pWinThread = AfxBeginThread(ReceiverThread, this, THREAD_PRIORITY_NORMAL);
#endif
	return 0;
}
