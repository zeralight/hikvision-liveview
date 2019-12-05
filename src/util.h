#ifndef DEF_UTIL_H
#define DEF_UTIL_H


#include <string>
#include <cstdint>

#include "winheaders.h"
#include "HCNetSDK.h"

namespace app {


std::string timeNow();

std::string get_current_time(); // yyyy-MM-dd-hh_hh-MM-SS

std::pair<int, int> getConfigResolution(BYTE resolution);
std::pair<int, int> getConfigResolution(const NET_DVR_COMPRESSIONCFG_V30& config);

int hex_to_int(uint16_t hex);
uint16_t int_to_hex(int n);

std::string winErrorStr(DWORD errorMessageID);
std::string message2str(UINT message);
bool ping(const char* src, int repeat=1);

template<typename T>
void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}


template<typename Function>
constexpr std::string get_function_name(Function f)
{
    void* g = reinterpret_cast<void*>(f);

    if (g == reinterpret_cast<void*>(ping)) return "ping";

    if (g == reinterpret_cast<void*>(::NET_DVR_Login_V40)) return "Login";
    if (g == reinterpret_cast<void*>(::NET_DVR_Logout)) return "Logout";
    if (g == reinterpret_cast<void*>(::NET_DVR_GetDVRConfig)) return "GetDVRConfig";
    if (g == reinterpret_cast<void*>(::NET_DVR_SetDVRConfig)) return "SetDVRConfig";
    if (g == reinterpret_cast<void*>(::NET_DVR_GetDeviceConfig)) return "GetDeviceConfig";
    
    return "undefined";
}


float translate_interval(float val, float a1, float b1, float a2, float b2);

}
#endif