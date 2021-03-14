#include "CTrackingInterfaceUdpSocket.h"

#include <sstream>
#include <string>
#include <vector>

UINT SenderThread(PVOID lpParam)
{
    CTrackingInterfaceUdpSocket * pObj = (CTrackingInterfaceUdpSocket*)lpParam;

    //int nCh = *((int*)lpParam);
    //int nIndex = nCh - 1;

    //printf("채널=%s TCP CMulticastSenderSocket 생성됨 \n", pObj->GetSocketName());

    SOCKET socket = pObj->GetSenderSocket();

    while (1)
    {
        if (pObj->IsSendData() == TRUE)
        {
        std:string strData = pObj->PopSendData();

            const int nSize = 256;
            BYTE byBuffer[nSize] = { 0, };
            memcpy(byBuffer, strData.c_str(), strData.length());



#if 1//멀티캐스트 전송
            sockaddr_in send_addr;
            memset(&send_addr, 0, sizeof(SOCKADDR_IN));
            send_addr.sin_family = AF_INET;
            send_addr.sin_addr.s_addr = inet_addr(pObj->GetDestAddr().c_str());					// this is our test destination IP
            send_addr.sin_port = htons(pObj->GetDestPort());
            if (sendto(socket, (const char*)&byBuffer, nSize, MSG_DONTROUTE, (sockaddr*)&send_addr, sizeof(send_addr)) == INVALID_SOCKET)
            {
                //TRACE("\nError in Sending data on the socket - %d", WSAGetLastError());
                WSACleanup();
                exit(-1);
            }
#endif

        }
        //LeaveCriticalSection(&g_csArrSendToAuthorityClient[nIndex]);
        Sleep(1);
    }


    return 0;
}

UINT ReceiverThread(PVOID lpParam)
{

    CTrackingInterfaceUdpSocket * pObj = (CTrackingInterfaceUdpSocket*)lpParam;

    //int nCh = *((int*)lpParam);
    //int nIndex = nCh - 1;

    //printf("채널=%s TCP CMulticastReceiverSocket 생성됨 \n", pObj->GetSocketName());

    
  

    SOCKET socket = pObj->GetRecverSocket();

    sockaddr_in receive_addr;		// Local socket's address. Holds the source address upon recvfrom function returns
    receive_addr.sin_family = AF_INET;
    //receive_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receive_addr.sin_addr.s_addr = inet_addr(pObj->GetHostAddr().c_str());
    receive_addr.sin_port = htons(pObj->GetHostPort());
    int nRecvAddrLen = sizeof(receive_addr);

    while (1)//종료 조건 추가 필요?
    {
        const int nSize = 256;
        BYTE byBuffer[nSize] = { 0, };
        int nRecvPacketLen = 0;
        if ((nRecvPacketLen = recvfrom(socket, (char*)byBuffer, nSize, 0, (struct sockaddr FAR *)&receive_addr, &nRecvAddrLen)) == SOCKET_ERROR)
        {
            //TRACE("\nrecvfrom failed: %d", WSAGetLastError());
            WSACleanup();
            exit(-1);
        }
        string recvStr((char*)byBuffer);
        pObj->SetRecvData(recvStr);
    }

    printf("\nSuccess: Leaving Multicast Group - %d", WSAGetLastError());

    closesocket(socket);
    WSACleanup();
    return TRUE;
    //LeaveCriticalSection(&g_csArrSendToAuthorityClient[nIndex]);

}



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



    if (m_hRecverThread) // 이미 Thread가 생성되어 있는 경우
    {
        if (WaitForSingleObject(m_hRecverThread, 0) == WAIT_TIMEOUT) { // Thread Still Running
            return 0;    // Start 거부
        }
        CloseHandle(m_hRecverThread);
    }

    // Thread 생성
    m_hSenderThread = CreateThread(
        NULL,            // default security attributes
        0,               // default stack size
        (LPTHREAD_START_ROUTINE)SenderThread,
        this,            // thread function argument = this class
        0,               // default creation flags
        &m_nRecverThreadID     // receive thread identifier
    );

    if (m_hSenderThread == NULL)
        return 0;
#endif








#if 1//Recver

    m_strHostAddr = strHostAddr;
    m_nHostPort = hostPort;
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
    receive_addr.sin_port = htons(hostPort);
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




    if (m_hRecverThread) // 이미 Thread가 생성되어 있는 경우
    {
        if (WaitForSingleObject(m_hRecverThread, 0) == WAIT_TIMEOUT) { // Thread Still Running
            return 0;    // Start 거부
        }
        CloseHandle(m_hRecverThread);
    }

    // Thread 생성
    m_hRecverThread = CreateThread(
        NULL,            // default security attributes
        0,               // default stack size
        (LPTHREAD_START_ROUTINE)ReceiverThread,
        this,            // thread function argument = this class
        0,               // default creation flags
        &m_nRecverThreadID     // receive thread identifier
    );

    if (m_hRecverThread == NULL)
        return 0;
#endif

    return 1;
}

string CTrackingInterfaceUdpSocket::MakeInterfaceFormat(int group, int left, int right, int top, int bottom, int videoWidth, int videoHeight, int status)
{
    char buffer[256];
    sprintf(buffer, "%d;%d;%d;%d;%d;%d;%d;%d;",
        group, left, right, top, bottom, videoWidth, videoWidth, status);

    return string(buffer);
}

void CTrackingInterfaceUdpSocket::ParseInterfaceFormat(
    string packet,
    int & groupNum,
    int & left, int & right, int & top, int & bottom, int & videoWidth, int & videoHeight, int & status)
{
    vector<string> result;
    string token;
    stringstream ss(packet);

    while (std::getline(ss, token, ';')) {
        result.push_back(token);
    }

    groupNum = atoi(result[0].c_str());
    left = atoi(result[1].c_str());
    right = atoi(result[2].c_str());
    top = atoi(result[3].c_str());
    bottom = atoi(result[4].c_str());
    videoWidth = atoi(result[5].c_str());
    videoHeight = atoi(result[6].c_str());
    status = atoi(result[7].c_str());
}


