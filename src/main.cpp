#include "winheaders.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "bgwin.h"
#include "button.h"
#include "cursors.h"
#include "globalwin.h"
#include "main.h"
#include "synchronized_ostream.h"
#include "trackbars.h"
#include "util.h"
#include "win.h"

#include "HCNetSDK.h"

#include <stdsoap2.h>

#include "soap/soapDeviceBindingProxy.h"
#include "soap/soapH.h"
#include "soap/soapMediaBindingProxy.h"
#include "soap/soapPTZBindingProxy.h"
#include "soap/soapPullPointSubscriptionBindingProxy.h"
#include "soap/soapRemoteDiscoveryBindingProxy.h"

#include <plugin/wsddapi.h>
#include <plugin/wsseapi.h>

namespace app {
configuration configuration::config_ = configuration();
synchronized_ostream clog{std::clog, true};
configuration config = configuration::get_instance();

}  // namespace app

using namespace app;

constexpr auto title = "Hikvision Live View";

static void usage(const boost::program_options::options_description &description,
                  const char *filename);
static void parse_input(int argc, char **argv);
static void list(const NET_DVR_DEVICEINFO_V40 &struDeviceInfoV40);
static bool stream(LONG uid, const NET_DVR_DEVICEINFO_V40 &struDeviceInfoV40);
static bool ptz(int pan, int tilt, int zoom);
static bool night_mode(const app::soap::IRMode &mode);
static bool record(bool start);
static void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser);
static bool alarm_input(int channel, bool open);
static bool alarm_output(int channel, int delay);
static void test_ping();

int main(int argc, char **argv) {
  std::showbase(clog);

  parse_input(argc, argv);

#if defined(DEBUG) || !defined(NDEBUG)
  test_ping();
#endif
  if (!NET_DVR_Init()) {
    std::cerr << ::NET_DVR_GetErrorMsg() << '\n';
    return 1;
  }

#if !defined(NDEBUG)
  char path[] = "logs/";
  if (!NET_DVR_SetLogToFile(3, path, false)) {
    std::cerr << "Error while creating logs: " << ::NET_DVR_GetErrorMsg() << '\n';
    return 1;
  }
#endif

  NET_DVR_USER_LOGIN_INFO struLoginInfo = {};
  std::strcpy(struLoginInfo.sDeviceAddress, config.host.c_str());
  struLoginInfo.wPort = config.port;
  std::strcpy(struLoginInfo.sUserName, config.username.c_str());
  std::strcpy(struLoginInfo.sPassword, config.password.c_str());

  std::cout << "Logging to the device...\n";
  NET_DVR_DEVICEINFO_V40 struDeviceInfoV40;

  if ((config.uid[0] = network_request(::NET_DVR_Login_V40, &struLoginInfo, &struDeviceInfoV40)) <
      0) {
    std::cerr << "Error Login: " << ::NET_DVR_GetErrorMsg() << '\n';
    ::NET_DVR_Cleanup();
    return 1;
  }
  std::cout << "Connected\n";

  app::soap::soap_thread.run();
  while (!app::soap::soap_thread.error() && !app::soap::soap_thread.connected())
    std::this_thread::sleep_for(std::chrono::seconds(1));
  if (app::soap::soap_thread.error()) {
    ::NET_DVR_Cleanup();
    std::cerr << "Onvif Error. Exiting\n";
    return 0;
  }
  // Set defaultconfig.channel
  if (config.channel == 0) config.channel = struDeviceInfoV40.struDeviceV30.byStartChan;

  int ret = 0;
  if (config.cmd == "list")
    list(struDeviceInfoV40);
  else if (config.cmd == "get") {
    ret = !stream(config.uid[0], struDeviceInfoV40);
  } else if (config.cmd == "pan" || config.cmd == "tilt" || config.cmd == "zoom") {
    ret = !ptz(config.pan, config.tilt, config.zoom);
  } else if (config.cmd == "IR-on") {
    ret = !night_mode(app::soap::IRMode::ON);
  } else if (config.cmd == "IR-off") {
    ret = !night_mode(app::soap::IRMode::OFF);
  } else if (config.cmd == "IR-auto") {
    ret = !night_mode(app::soap::IRMode::AUTO);
  } else if (config.cmd == "record-start") {
    ret = !record(true);
  } else if (config.cmd == "record-stop") {
    ret = !record(false);
  } else if (config.cmd == "alarm-in-open") {
    ret = !alarm_input(config.alarm_channel, true);
  } else if (config.cmd == "alarm-in-close") {
    ret = !alarm_input(config.alarm_channel, false);
  } else if (config.cmd == "alarm-out-delay") {
    ret = !alarm_output(config.alarm_channel, config.alarm_delay);
  }
  app::soap::soap_thread.must_exit();
  app::soap::soap_thread.thread().join();

  network_request(::NET_DVR_Logout, config.uid[0]);
  ::NET_DVR_Cleanup();

  return ret;
}

static void usage(const boost::program_options::options_description &description,
                  const char *filename) {
  char fname[_MAX_FNAME + 1];
  _splitpath(filename, nullptr, nullptr, fname, nullptr);
  std::cout << "Usage:\n";
  std::cout << fname << ".exe "
            << "list host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "get host port http-username http-password onvif-username onvif-password "
               "[channel-to-stream]"
            << " [Pan/Tilt-sensitivity] [Zooming - sensitivity]\n";
  std::cout
      << fname << ".exe "
      << "pan host port http-username http-password onvif-username onvif-password -P pan-value\n";
  std::cout
      << fname << ".exe "
      << "tilt host port http-username http-password onvif-username onvif-password -T tilt-value\n";
  std::cout
      << fname << ".exe "
      << "zoom host port http-username http-password onvif-username onvif-password -Z zoom-value\n";
  std::cout << fname << ".exe "
            << "IR-on host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "IR-off host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "IR-auto host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "record-start host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "record-stop host port http-username http-password onvif-username onvif-password\n";
  std::cout << fname << ".exe "
            << "alarm-in-open host port http-username http-password onvif-username onvif-password "
               "[-A | --alarm-channel] alarm-channel\n";
  std::cout << fname << ".exe "
            << "alarm-in-close host port http-username http-password onvif-username onvif-password "
               "[-A | --alarm-channel] alarm-channel\n";
  std::cout
      << fname << ".exe "
      << "alarm-out-delay host port http-username http-password onvif-username onvif-password "
         "[-A | --alarm-channel] [--alarm-delay | -d] alarm-delay\n";
  std::cout << description << '\n';
}

static void parse_input(int argc, char **argv) {
  namespace po = boost::program_options;

  po::options_description description("Allowed options");
  description.add_options()("help,h", "prints this")(
      "command,c", po::value<std::string>(&config.cmd)->required(), "list or get")(
      "host,H", po::value<std::string>(&config.host)->required(), "server address")(
      "port,p", po::value<uint16_t>(&config.port)->required(), "Hikvision Protocol port")(
      "http-port,t", po::value<uint16_t>(&config.soap_port)->required(), "Onvif HTTP port")(
      "username,u", po::value<std::string>(&config.username)->required(), "username")(
      "password,P", po::value<std::string>(&config.password)->required(), "password")(
      "onvif-username,U", po::value<std::string>(&config.onvif_username)->required(),
      "Onvif username")("onvif-password,a",
                        po::value<std::string>(&config.onvif_password)->required(),
                        "Onvif password")(
      "channel,c", po::value<int>(&config.channel)->default_value(0), "channel Number")(
      "pt-sensitivity,s", po::value<double>(&config.p_sensitivity)->default_value(1.),
      "Pan Sensitivity (> 0.0)")(
      "z-sensitivity,z", po::value<int>(&config.z_sensitivity)->default_value(1),
      "Zoom Sensitivity (>= 1)")("stream,S", po::value<int>(&config.stream_type)->default_value(0),
                                 "Stream Type (0: Main-Stream. 1: Sub-Stream, ...)")(
      "rtt,R", po::value<int>(&config.round_trip_time)->default_value(50),
      "Round Trip Time (in ms)")("pan,P", po::value<int>(&config.pan),
                                 "Pan distance ([-100, 100])")(
      "tilt,T", po::value<int>(&config.tilt), "Tilt distance ([-100, 100])")(
      "zoom,Z", po::value<int>(&config.zoom), "Zoom distance ([-100, 100])")(
      "record-dir,D", po::value<std::string>(&config.record_dir)->default_value(std::string{"."}),
      "Recording directory (default current directory)")(
      "alarm-channel,A", po::value<int>(&config.alarm_channel),
      "The Alarm channel Number (0 -> 1st alarm channel, 1 -> 2nd one, and so on)")(
      "alarm-delay,d", po::value<int>(&config.alarm_delay),
      "The Delay of alarm out: 0 -> 5s, 1 -> 10s, 2 -> 30s, 3 -> 1 minute, 4 -> 2 minutes, 5 -> 5 "
      "minutes, 6 -> 10 minutes, 7 -> manual");

  po::positional_options_description p;
  p.add("command", 1)
      .add("host", 1)
      .add("port", 1)
      .add("http-port", 1)
      .add("username", 1)
      .add("password", 1)
      .add("onvif-username", 1)
      .add("onvif-password", 1)
      .add("config.channel", 1)
      .add("pt-sensitivity", 1)
      .add("z-sensitivity", 1)
      .add("pan", 1)
      .add("tilt", 1)
      .add("zoom", 1)
      .add("record-dir", 1)
      .add("alarm-channel", 1)
      .add("alarm-delay", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(description).positional(p).run(), vm);
    if (vm.count("help")) {
      usage(description, argv[0]);
      std::exit(0);
    }
    po::notify(vm);
    if (config.cmd != "list" && config.cmd != "get" && config.cmd != "pan" &&
        config.cmd != "tilt" && config.cmd != "zoom" && config.cmd != "IR-on" &&
        config.cmd != "IR-off" && config.cmd != "IR-auto" && config.cmd != "record-start" &&
        config.cmd != "record-stop" && config.cmd != "alarm-in-open" &&
        config.cmd != "alarm-in-close" && config.cmd != "alarm-out-delay")
      throw std::runtime_error("The option " + config.cmd + " is invalid.");
    if (config.cmd == "pan" && !vm.count("pan"))
      throw std::runtime_error("The Pan distance must be set");
    if (config.cmd == "pan" && (config.pan < -100 || config.pan > 100))
      throw std::runtime_error("The pan distance must be in [-100, 100]");
    if (config.cmd == "tilt" && !vm.count("tilt"))
      throw std::runtime_error("The Tilt distance must be set");
    if (config.cmd == "tilt" && (config.tilt < -100 || config.tilt > 100))
      throw std::runtime_error("The tilt distance must be in [-100, 100]");
    if (config.cmd == "zoom" && !vm.count("zoom"))
      throw std::runtime_error("The zoom distance must be set");
    if (config.cmd == "zoom" && (config.zoom < -100 || config.zoom > 100))
      throw std::runtime_error("The zoom distance must be in [-100, 100]");
    if ((config.cmd == "alarm-in-open" || config.cmd == "alarm-in-close" ||
         config.cmd == "alarm-out-delay") &&
        !vm.count("alarm-channel"))
      throw std::runtime_error("The alarm-channel argument is required");
    if (config.cmd == "alarm-out-delay" && !vm.count("alarm-delay"))
      throw std::runtime_error("The alarm-delay argument is required");
    if (config.cmd == "alarm-out-delay" && config.alarm_delay < 0 || config.alarm_delay > 7)
      throw std::runtime_error("The alarm-delay argument must be between 0 and 7");
    if (config.p_sensitivity < std::numeric_limits<double>::epsilon())
      throw std::runtime_error("The Pan/Tilt sensitivity is too low");
    if (config.z_sensitivity < 1) throw std::runtime_error("The Z sensitivity must be >= 1");
  } catch (const std::exception &e) {
    std::cout << e.what() << '\n';
    usage(description, argv[0]);
    std::exit(0);
  }
  config.t_sensitivity = 0.6 * config.p_sensitivity;
}

static void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG, LONG, void *) {
  switch (dwType) {
    case EXCEPTION_RECONNECT:  // Reconnect in live view
      std::cerr << "----------Reconnecting--------\n";
      break;
    default:
      break;
  }
}

static void list(const NET_DVR_DEVICEINFO_V40 &struDeviceInfoV40) {
  std::cout << "Fetching Device channels ...\n";

  const auto &dev = struDeviceInfoV40.struDeviceV30;
  std::vector<int> channels;
  for (int i = dev.byStartChan; i < dev.byStartChan + dev.byChanNum; ++i) channels.push_back(i);
  size_t idxAnalog = channels.size();
  for (int i = dev.byStartDChan;
       i < int(dev.byStartDChan) + int(dev.byIPChanNum) + 256 * int(dev.byHighDChanNum); ++i)
    channels.push_back(i);

  std::cout << "Analog channels:\n";
  for (size_t i = 0; i < idxAnalog; ++i) std::cout << channels[i] << '\n';
  std::cout << "Digital channels:\n";
  for (size_t i = idxAnalog; i < channels.size(); ++i) std::cout << channels[i] << '\n';
}

static bool stream(LONG uid, const NET_DVR_DEVICEINFO_V40 &struDeviceInfoV40) {
  clog.log("channel to stream: ", config.channel);

  // Set callback for exceptions
  NET_DVR_SetExceptionCallBack_V30(0, NULL, g_ExceptionCallBack, NULL);

  // Getting Resolution
  std::cout << "Reading channel Resolution...\n";
  DWORD dw;
  NET_DVR_MULTI_STREAM_COMPRESSIONCFG_COND compression_params = {};
  compression_params.dwSize = sizeof compression_params;
  compression_params.struStreamInfo.dwSize = compression_params.struStreamInfo.dwSize;
  std::memset(compression_params.struStreamInfo.byID, 0, 32);
  compression_params.struStreamInfo.dwChannel = config.channel;
  compression_params.dwStreamType = config.stream_type;
  NET_DVR_MULTI_STREAM_COMPRESSIONCFG compression_settings;
  uint32_t status;
  if (!network_request(::NET_DVR_GetDeviceConfig, uid, NET_DVR_GET_MULTI_STREAM_COMPRESSIONCFG, 1,
                       &compression_params, sizeof compression_params, &status,
                       &compression_settings, sizeof compression_settings)) {
    std::cerr << "Reading Channel Resolution Failed. " << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }
  clog.log("Stream type:", compression_settings.dwStreamType,
           " | Resolution :", (int)compression_settings.struStreamPara.byResolution);
  config.streamResolution = getConfigResolution(compression_settings.struStreamPara.byResolution);
  if (config.streamResolution.first == -1) {
    std::cerr
        << "Unrecognized response (decoding not implemented yet). Check the documentation for "
           "NET_DVR_COMPRESSION_INFO_V30"
        << " and make the required changes in util.h).\n"
        << "Using Default Resolution 1024x768\n";
    config.streamResolution = {1024, 768};
  }

  std::cout << "Resolution: " << config.streamResolution.first << " x "
            << config.streamResolution.second << '\n';

  RECT rect = {0, 0, config.streamResolution.first, config.streamResolution.second};
  if (!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false)) {
    std::cerr << winErrorStr(::GetLastError()) << '\n';
    return false;
  }

  std::cout << "Initializing Display ...\n";
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

  auto &&cursor = ::bitmap2cursor(mat2bitmap(cursor_matrix), RGB(0xff, 0xff, 0xff),
                                  cursor_matrix.size() / 2, cursor_matrix[0].size() / 2);

  const auto global_w = rect.right - rect.left;
  const auto global_h = rect.bottom - rect.top;

  GlobalWindow global_win;
  if (!global_win.Create(title, WS_OVERLAPPEDWINDOW, 0, 0, 0, global_w, global_h)) {
    std::cerr << winErrorStr(::GetLastError()) << '\n';
    return false;
  }

  BGWindow bgwin;
  if (!bgwin.Create(title, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, 0, global_win.Window(), nullptr,
                    cursor)) {
    std::cerr << winErrorStr(::GetLastError()) << '\n';
    return false;
  }
  global_win.bgwin() = &bgwin;
  bgwin.globalwin() = &global_win;

  MainWindow win;
  if (!win.Create(title, WS_POPUP | WS_VISIBLE,
                  WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, 0, 0, 0, 0,
                  bgwin.Window())) {
    std::cerr << winErrorStr(::GetLastError()) << '\n';
    return false;
  }
  bgwin.DrawingWindow() = &win;
  win.BackgroundWindow() = &bgwin;

  if (!(::SetLayeredWindowAttributes(win.Window(), RGB(0xff, 0xff, 0xff), 0xff,
                                     LWA_COLORKEY | LWA_ALPHA))) {
    std::cerr << "SetLayeredWindowAttributes Failed: " << winErrorStr(GetLastError()) << '\n';
  }

  // Start Streaming
  clog.log("stream type = ", config.stream_type);
  std::cout << "Starting Streaming" << std::endl;
  NET_DVR_PREVIEWINFO struPlayInfo = {};
  struPlayInfo.hPlayWnd = bgwin.Window();
  struPlayInfo.lChannel = config.channel;
  struPlayInfo.dwStreamType = config.stream_type;
  struPlayInfo.dwLinkMode = 1;
  struPlayInfo.bBlocked = 0;
  if ((config.real_play_handle = NET_DVR_RealPlay_V40(uid, &struPlayInfo, nullptr, nullptr)) < 0) {
    std::cerr << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }
  ::ShowWindow(global_win.Window(), 1);
  global_win.visible() = true;
  // FreeConsole();

  MSG msg = {};
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  return true;
}

static bool ptz(int pan, int tilt, int zoom) {
  clog.log("main:ptz pan = ", pan, " | tilt = ", tilt, " | zoom = ", zoom);
  namespace soap = app::soap;
  std::mutex mx;
  std::condition_variable cv;
  bool done = false;
  class Runner : public soap::SoapActionRunnerAdapter {
    std::mutex &mx_;
    std::condition_variable &cv_;
    bool &done_;

   public:
    Runner(std::mutex &mx, std::condition_variable &cv, bool &done)
        : soap::SoapActionRunnerAdapter(), mx_(mx), cv_(cv), done_(done) {}
    virtual void soap_relative_move_is_done(soap::SoapRelativeMoveAction *action) override {
      std::unique_lock<std::mutex> lock(mx_);
      done_ = true;
      cv_.notify_all();
    }
  } runner(mx, cv, done);
  soap::SoapRelativeMoveAction *action = new soap::SoapRelativeMoveAction(
      soap::soap_thread, runner, pan / 100.f, tilt / 100.f, zoom / 100.f);
  soap::soap_thread.queue(action);
  std::unique_lock<std::mutex> lock(mx);
  cv.wait(lock, [&done]() { return done; });
  std::cout << "Done\n";
  // delete action;

  return true;
}

static bool night_mode(const app::soap::IRMode &mode) {
  clog.log("main::night_mode = ", mode);
  namespace soap = app::soap;
  std::mutex mx;
  std::condition_variable cv;
  bool done = false;
  class Runner : public soap::SoapActionRunnerAdapter {
    std::mutex &mx_;
    std::condition_variable &cv_;
    bool &done_;

   public:
    Runner(std::mutex &mx, std::condition_variable &cv, bool &done)
        : soap::SoapActionRunnerAdapter(), mx_(mx), cv_(cv), done_(done) {}
    virtual void soap_night_mode_is_done(soap::SoapIRModeAction *action) override {
      std::unique_lock<std::mutex> lock(mx_);
      done_ = true;
      cv_.notify_all();
    }
  } runner(mx, cv, done);

  soap::SoapIRModeAction *action = new soap::SoapIRModeAction(soap::soap_thread, runner, mode);
  soap::soap_thread.queue(action);
  std::unique_lock<std::mutex> lock(mx);
  cv.wait(lock, [&done]() { return done; });
  // delete action;
  return true;
}

static bool record(bool start) {
  bool ret = true;
  if (start) {
    if (!::NET_DVR_StartDVRRecord(config.uid[0], config.channel, 0)) {
      std::cerr << "Error when starting remote record: " << ::NET_DVR_GetErrorMsg() << '\n';
      ret = false;
    } else {
      std::cout << "Started remote record\n";
    }
  } else {
    if (!::NET_DVR_StopDVRRecord(config.uid[0], config.channel)) {
      std::cerr << "Error when stopping remote record: " << ::NET_DVR_GetErrorMsg() << '\n';
      ret = false;
    } else {
      std::cout << "Stopped remote record\n";
    }
  }

  return ret;
}

static bool alarm_input(int channel, bool open) {
  clog.log("Alarm input: ", channel, " | ", open);

  ::NET_DVR_ALARMINCFG net_dvr_alarmin_cfg;

  DWORD dw;
  if (!::NET_DVR_GetDVRConfig(config.uid[0], NET_DVR_GET_ALARMINCFG, channel, &net_dvr_alarmin_cfg,
                              sizeof net_dvr_alarmin_cfg, &dw)) {
    std::cerr << "Reading Alarm input configuration [channel = " << channel
              << "] failed: " << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }

  net_dvr_alarmin_cfg.byAlarmType = !open;
  if (!::NET_DVR_SetDVRConfig(config.uid[0], NET_DVR_SET_ALARMINCFG, channel, &net_dvr_alarmin_cfg,
                              sizeof net_dvr_alarmin_cfg)) {
    std::cerr << "Setting Alarm input parameter [channel = " << channel << ", open = " << open
              << "] failed: " << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }

  std::cout << "Alarm input No " << channel << ' ' << (open ? "opened" : "closed")
            << " successfully\n";
  return true;
}

static bool alarm_output(int channel, int delay) {
  clog.log("Alarm output: ", channel, " | ", delay);

  ::NET_DVR_ALARMOUTCFG net_dvr_alarmout_cfg;

  DWORD dw;
  if (!::NET_DVR_GetDVRConfig(config.uid[0], NET_DVR_GET_ALARMOUTCFG, channel,
                              &net_dvr_alarmout_cfg, sizeof net_dvr_alarmout_cfg, &dw)) {
    std::cerr << "Reading Alarm output configuration [channel = " << channel
              << "] failed: " << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }

  clog.log("alarm out current delay = ", net_dvr_alarmout_cfg.dwAlarmOutDelay);
  net_dvr_alarmout_cfg.dwAlarmOutDelay = delay;
  clog.log("alarm out new delay = ", net_dvr_alarmout_cfg.dwAlarmOutDelay);
  if (!::NET_DVR_SetDVRConfig(config.uid[0], NET_DVR_GET_ALARMOUTCFG, channel,
                              &net_dvr_alarmout_cfg, sizeof net_dvr_alarmout_cfg)) {
    std::cerr << "Setting Alarm output delay [channel = " << channel << ", delay = " << delay
              << "] failed: " << ::NET_DVR_GetErrorMsg() << '\n';
    return false;
  }

  std::cout << "Alarm output No " << channel << " delay changed successfully\n";
  return true;
}

static void test_ping() {
  clog.log("Measuring Ping duration");
  for (int i : {0, 1, 2}) {
    if (!network_request(ping, config.host.c_str(), 1)) {
      auto err = ::GetLastError();
      std::cerr << "Ping Measure Failure: ";
      if (!err)
        std::cerr << "Timeout.\n";
      else
        std::cerr << winErrorStr(::GetLastError()) << '\n';
    }
  }
  clog.log("Done");
}