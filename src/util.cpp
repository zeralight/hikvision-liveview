#include "util.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>

#include "HCNetSDK.h"
#include "main.h"
#include "winheaders.h"

namespace app {

std::string timeNow() {
  using namespace std::chrono;

  auto now = std::chrono::high_resolution_clock::now();
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
  std::time_t now_time = std::chrono::high_resolution_clock::to_time_t(now);
  char str[26];
  ctime_s(str, sizeof str, &now_time);

  std::string pad = std::to_string(ms);
  while (pad.size() != 3) pad = '0' + pad;

  std::string ret = str;
  return ret.substr(0, ret.size() - 6) + ':' + pad;
}

// yyyy-MM-dd-hh_hh-MM-SS
std::string get_current_time() {
  namespace po = boost::program_options;
  namespace pt = boost::posix_time;
  namespace gr = boost::gregorian;
  const auto current_time = pt::ptime(pt::second_clock::local_time());
  std::stringstream ss;
  ss << current_time.date().year() << '-' << current_time.date().month().as_number() << '-' << current_time.date().day() << '_'
     << current_time.time_of_day().hours() << '-' << current_time.time_of_day().minutes() << '-' << current_time.time_of_day().seconds();

  return ss.str();
}

std::pair<int, int> getConfigResolution(BYTE resolution) {
  switch (resolution) {
    case 1:
      return {352, 240};
    case 2:
      return {176, 120};
    case 3:
      return {704, 480};
    case 4:
      return {704, 240};
    case 6:
      return {320, 240};
    case 7:
      return {160, 120};
    case 12:
      return {384, 288};
    case 13:
      return {576, 576};
    case 16:
      return {640, 480};
    case 17:
      return {1600, 1200};
    case 18:
      return {800, 600};
    case 19:
      return {1280, 720};
    case 20:
      return {1280, 960};
    // case 21
    case 27:
      return {1920, 1080};
    default:
      return {-1, -1};
  }
}

std::pair<int, int> getConfigResolution(const ::NET_DVR_COMPRESSIONCFG_V30& config) { return getConfigResolution(config.struNormHighRecordPara.byResolution); }

int hex_to_int(uint16_t hex) {
  int a = hex >> 12;
  int b = (hex >> 8) % 16;
  int c = (hex >> 4) % 16;
  int d = hex % 16;

  return 10 * (10 * (10 * a + b) + c) + d;
}

uint16_t int_to_hex(int n) {
  uint8_t d = n % 10;
  uint8_t c = (n / 10) % 10;
  uint8_t b = (n / 100) % 10;
  uint8_t a = n / 1000;
  return (((((a << 4) + b) << 4) + c) << 4) + d;
}

std::string message2str(UINT message) {
  if (message == WM_LBUTTONDOWN) return "LEFT BUTTON CLICK";
  if (message == WM_LBUTTONUP) return "LEFT BUTTON RELEASE";
  if (message == WM_MOUSEMOVE) return "MOUSE MOVE";
  if (message == WM_MOUSEWHEEL) return "MOUSE WHEEL";

  if (message == WM_TIMER) return "TIMER EXPIRED";

  if (message == WM_PAINT) return "PAINT";

  if (message == WM_DESTROY) return "DESTROY";

  return "undefined (" + std::to_string(message) + ")";
}

std::string winErrorStr(DWORD errorMessageID) {
  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

  std::string message(messageBuffer, size);

  LocalFree(messageBuffer);

  return message;
}

bool ping(const char* src, int repeat) {
  char SendData[] = "";

  std::vector<char> reply_vec(sizeof(ICMP_ECHO_REPLY) + repeat * sizeof(SendData));
  void* reply_buffer = static_cast<void*>(reply_vec.data());

  unsigned long ipaddr = INADDR_NONE;
  ipaddr = inet_addr(src);
  if (ipaddr == INADDR_NONE) {
    clog.log("ping: invalid ip");
    return false;
  }

  HANDLE handle = IcmpCreateFile();
  if (handle == INVALID_HANDLE_VALUE) return false;

  char send_buffer[] = "";
  if (!::IcmpSendEcho(handle, ipaddr, send_buffer, sizeof send_buffer, nullptr, reply_buffer, reply_vec.size(), 10000)) return false;

  return true;
}

float translate_interval(float val, float a1, float b1, float a2, float b2) {
  const auto x = (b2 - b1) / (a2 - a1);
  const auto y = b1 - x * a1;
  return val * x + y;
}

}  // namespace app