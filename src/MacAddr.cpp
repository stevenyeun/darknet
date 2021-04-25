#include "MacAddr.h"

#include <iostream>
#include <Windows.h>
#include <iphlpapi.h>
#include <memory>
#include <string>

#include <algorithm>

using namespace std;

#pragma comment (lib, "Iphlpapi.lib")

template<typename ... Args>
std::string format_string(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
	std::unique_ptr<char[]> buffer(new char[size]);
	snprintf(buffer.get(), size, format.c_str(), args ...);
	return std::string(buffer.get(), buffer.get() + size - 1);
}

std::vector<std::string> getMacAddress()
{
	vector<std::string> vecMacAddr;

	ULONG buffer_length = 0;

	if (::GetAdaptersInfo(NULL, &buffer_length) == ERROR_BUFFER_OVERFLOW) {

		std::string adapter_info_string;

		char *p_adapter = new char[buffer_length];

		IP_ADAPTER_INFO *p_pos;

		if (GetAdaptersInfo((IP_ADAPTER_INFO *)p_adapter, &buffer_length) == ERROR_SUCCESS) {

			p_pos = (IP_ADAPTER_INFO *)p_adapter;

			while (p_pos != NULL) {

				adapter_info_string = format_string("%s(%s) : ",
					p_pos->Description, p_pos->IpAddressList.IpAddress.String);

				// MAC 주소를 16진수 형태로 출력한다. 예) AA-BB-CC-DD-EE-FF
				std::string mac;
				for (unsigned int i = 0; i < p_pos->AddressLength; i++) {

					unsigned int temp = (p_pos->Address[i] & 0x000000FF);
					mac += format_string("%02X", temp);

					/*if (i < p_pos->AddressLength - 1)
						mac += "-";

					adapter_info_string += str;*/
				}

				//버추얼이 아닌 맥 어드레스만 저장
				std::transform(adapter_info_string.cbegin(), adapter_info_string.cend(),
					adapter_info_string.begin(), tolower);

				if (adapter_info_string.find("virtual") == -1)
				{
					vecMacAddr.push_back(mac);
				}


				p_pos = p_pos->Next; // 다음 어뎁터를 위한 정보로 이동한다. 
			}
		}
		delete[] p_adapter;
	}

	return vecMacAddr;
}