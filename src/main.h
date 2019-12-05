#ifndef DEF_MAIN_H
#define DEF_MAIN_H

#include <array>
#include <queue>

#include <utility>

#include <stdsoap2.h>

#include "soap/soapDeviceBindingProxy.h"
#include "soap/soapMediaBindingProxy.h"
#include "soap/soapPTZBindingProxy.h"

#include "HCNetSDK.h"
#include "winheaders.h"

#include "Consumer.h"
#include "network_requests.h"
#include "soap.h"

namespace app {

struct configuration {
 private:
  configuration() = default;

  static configuration config_;

 public:
  static configuration& get_instance() { return config_; }

  std::string cmd;
  std::string host;
  uint16_t port;
  std::string username;
  std::string password;

  std::pair<int, int> streamResolution;

  std::array<LONG, 3> uid;

  int channel;

  double p_sensitivity;
  double t_sensitivity;

  int z_sensitivity;

  int stream_type;
  LONG real_play_handle;
  
  int round_trip_time;

  NET_DVR_PTZPOS ptz;

  std::string onvif_username;
  std::string onvif_password;
  uint16_t soap_port;

  int pan;
  int tilt;
  int zoom;

  std::string record_dir;

  int alarm_channel;
  int alarm_delay;
};

extern configuration config;
}  // namespace app

extern SOAP_NMAC struct Namespace namespaces[];

#endif