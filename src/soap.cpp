
#include <cassert>
#include <sstream>

#include <stdsoap2.h>

#include "soap/soapDeviceBindingProxy.h"
#include "soap/soapH.h"
#include "soap/soapMediaBindingProxy.h"
#include "soap/soapPTZBindingProxy.h"
#include "soap/soapPullPointSubscriptionBindingProxy.h"
#include "soap/soapRemoteDiscoveryBindingProxy.h"

#include <plugin/wsddapi.h>
#include <plugin/wsseapi.h>

#include "exceptions.h"
#include "main.h"
#include "synchronized_ostream.h"
#include "util.h"

namespace app {
namespace soap {

SoapThread soap_thread;
SoapActionRunnerNothing do_nothing_runner;


std::ostream &operator<<(std::ostream &out, const IRMode &state) {
  if (state == IRMode::ON)
    out << "IRMode::ON";
  else if (state == IRMode::OFF)
    out << "IRMode::OFF";
  else if (state == IRMode::AUTO)
    out << "IRMode::AUTO";
  else
    out << "IRMode::unrecognized_state";

  return out;
}

/******************************************************************************\
 *
 *	SoapThread
 *
 \******************************************************************************/

SoapThread::SoapThread()
    : waiting_for_data_(false),
      connected_(false),
      error_(false),
      exit_(false),
      soap(soap_new1(SOAP_XML_CANONICAL)),
      proxy_device_(soap),
      proxy_media_(soap),
      proxy_ptz_(soap),
      proxy_imaging_(soap) {}

bool SoapThread::init() {
  soap_endpoint = std::string("http://") + config.host + ":" + std::to_string(config.soap_port) +
                  "/onvif/device_service";
  soap->connect_timeout = soap->recv_timeout = soap->send_timeout = 30;  // 30 sec
  ::soap_register_plugin(soap, ::soap_wsse);
  clog.log("soap_endpoint = ", soap_endpoint);
  proxy_device_.soap_endpoint = soap_endpoint.c_str();

  ::_tds__GetDeviceInformation GetDeviceInformation;
  ::_tds__GetDeviceInformationResponse GetDeviceInformationResponse;
  set_credentials();
  if (proxy_device_.GetDeviceInformation(&GetDeviceInformation, GetDeviceInformationResponse)) {
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  /* ::check_response(soap); */
  clog.log("Manufacturer:    ", GetDeviceInformationResponse.Manufacturer);
  clog.log("Model:           ", GetDeviceInformationResponse.Model);
  clog.log("FirmwareVersion: ", GetDeviceInformationResponse.FirmwareVersion);
  clog.log("SerialNumber:    ", GetDeviceInformationResponse.SerialNumber);
  clog.log("HardwareId:      ", GetDeviceInformationResponse.HardwareId);

  // get device capabilities and print media
  _tds__GetCapabilities GetCapabilities;
  _tds__GetCapabilitiesResponse GetCapabilitiesResponse;
  set_credentials();
  if (proxy_device_.GetCapabilities(&GetCapabilities, GetCapabilitiesResponse)) {
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  /* ::check_response(soap); */
  if (!GetCapabilitiesResponse.Capabilities || !GetCapabilitiesResponse.Capabilities->Media ||
      !GetCapabilitiesResponse.Capabilities->PTZ ||
      !GetCapabilitiesResponse.Capabilities->Imaging) {
    std::cerr << "Missing device capabilities info" << std::endl;
    return false;
  }
  proxy_media_.soap_endpoint = GetCapabilitiesResponse.Capabilities->Media->XAddr.c_str();
  proxy_ptz_.soap_endpoint = GetCapabilitiesResponse.Capabilities->PTZ->XAddr.c_str();
  proxy_imaging_.soap_endpoint = GetCapabilitiesResponse.Capabilities->Imaging->XAddr.c_str();
  clog.log("Media XAddr: ", proxy_media_.soap_endpoint);
  clog.log("PTZ XAddr: ", proxy_ptz_.soap_endpoint);
  clog.log("IMAGING XAddr: ", proxy_imaging_.soap_endpoint);

  // get device profiles
  ::_trt__GetProfiles GetProfiles;
  ::_trt__GetProfilesResponse GetProfilesResponse;
  set_credentials();
  if (proxy_media_.GetProfiles(&GetProfiles, GetProfilesResponse)) {
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  soap_profile_ = GetProfilesResponse.Profiles[0];
  soap_ptz_configuration_ = soap_profile_->PTZConfiguration;
  profile_token_ = soap_profile_->token;
  video_source_token_ = soap_profile_->VideoSourceConfiguration->SourceToken;
  audio_source_token_ = soap_profile_->AudioSourceConfiguration->SourceToken;
  ::_trt__GetAudioOutputs *trt__GetAudioOutputs = ::soap_new__trt__GetAudioOutputs(soap);
  ::_trt__GetAudioOutputsResponse trt__GetAudioOutputsResponse;
  set_credentials();
  if (proxy_media_.GetAudioOutputs(trt__GetAudioOutputs, trt__GetAudioOutputsResponse)) {
    std::cerr << "Error when Reading Audio configuration:\n";
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  audio_main_output_token_ = trt__GetAudioOutputsResponse.AudioOutputs[0]->token;

  // _trt__GetAudioOutputConfigurationOptions *trt__GetAudioOutputConfigurationOptions =
  //     ::soap_new_set__trt__GetAudioOutputConfigurationOptions(soap, nullptr, &profile_token_);
  // _trt__GetAudioOutputConfigurationOptionsResponse
  // trt__GetAudioOutputConfigurationOptionsResponse; set_credentials(); if
  // (proxy_media_.GetAudioOutputConfigurationOptions(trt__GetAudioOutputConfigurationOptions,
  //                                                     trt__GetAudioOutputConfigurationOptionsResponse))
  //                                                     {
  //   std::cerr << "Error when Reading Audio Options:\n";
  //   ::soap_stream_fault(soap, std::cerr);
  //   return false;
  // }
  // main_audio_output_level_min_ =
  // trt__GetAudioOutputConfigurationOptionsResponse.Options->OutputLevelRange->Min;
  // main_audio_output_level_max_ =
  // trt__GetAudioOutputConfigurationOptionsResponse.Options->OutputLevelRange->Max;

  // clog.log("audio output level range = [", main_audio_output_level_min_, ", ",
  // main_audio_output_level_max_, "]"); clog.log("pan [min, max] = [", pan_min(), ", ", pan_max,
  // "]"); clog.log("tilt [min, max] = [", tilt_min(), ", ", tilt_max, "]"); clog.log("zoom [min,
  // max] = [", zoom_min(), ", ", zoom_max, "]");

  // _trt__GetAudioOutputConfigurations *trt__GetAudioOutputConfigurations =
  // ::soap_new_set__trt__GetAudioOutputConfigurations(soap);
  // _trt__GetAudioOutputConfigurationsResponse trt__GetAudioOutputConfigurationsResponse;
  // set_credentials();
  // if (proxy_media_.GetAudioOutputConfigurations(trt__GetAudioOutputConfigurations,
  // trt__GetAudioOutputConfigurationsResponse)) {
  //   std::cerr << "Error when Reading Audio Configurations:\n";
  //   ::soap_stream_fault(soap, std::cerr);
  //   return false;
  // }
  // tt__AudioOutputConfiguration* AudioOutputConfiguration =
  // trt__GetAudioOutputConfigurationsResponse.Configurations[0]; clog.log("Output audio volume: ",
  // AudioOutputConfiguration->OutputLevel);

  // _trt__SetAudioOutputConfiguration *trt__SetAudioOutputConfiguration =
  // ::soap_new_set__trt__SetAudioOutputConfiguration(soap, AudioOutputConfiguration,
  // ::soap_new_bool(soap, true)); _trt__SetAudioOutputConfigurationResponse
  // trt__SetAudioOutputConfigurationResponse; AudioOutputConfiguration->OutputLevel = 9;
  // set_credentials();
  // if (proxy_media_.SetAudioOutputConfiguration(trt__SetAudioOutputConfiguration,
  // trt__SetAudioOutputConfigurationResponse)) {
  //   std::cerr << "Error when Reading Audio Configurations:\n";
  //   ::soap_stream_fault(soap, std::cerr);
  //   return false;
  // }
  // clog.log("Setting Audio Output Configuration: done");

  return true;
}

bool SoapThread::start_move(float dx, float dy) {
  clog.log("Soap: Starting Move: dx = ", dx, " | dy = ", dy);
  ::_tptz__ContinuousMove *tptz__ContinuousMove = ::soap_new_req__tptz__ContinuousMove(
      soap, profile_token_,
      ::soap_new_set_tt__PTZSpeed(soap, ::soap_new_set_tt__Vector2D(soap, dx, dy, nullptr),
                                  nullptr));
  ::_tptz__ContinuousMoveResponse tptz__ContinuousMoveResponse;
  set_credentials();
  if (proxy_ptz_.ContinuousMove(tptz__ContinuousMove, tptz__ContinuousMoveResponse)) {
    std::cerr << "Error when Starting continuous move operation:\n";
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }

  return true;
}

bool SoapThread::stop_move() {
  clog.log("Soap: Stopping Move");
  ::_tptz__Stop *tptz__Stop = ::soap_new_set__tptz__Stop(
      soap, profile_token_, ::soap_new_bool(soap, true), ::soap_new_bool(soap, true));
  ::_tptz__StopResponse tptz__StopResponse;
  set_credentials();
  if (proxy_ptz_.Stop(tptz__Stop, tptz__StopResponse)) {
    std::cerr << "Error when Stopping continuous move" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }

  return true;
}

bool SoapThread::move_to(float p, float t, float z) {
  clog.log("Starting Absolute move to p = ", p, " | t = ", t, " | z  = ", z);
  auto &&ptz_vec =
      ::soap_new_set_tt__PTZVector(soap, ::soap_new_set_tt__Vector2D(soap, p, t, nullptr),
                                   ::soap_new_set_tt__Vector1D(soap, z, nullptr));
  _tptz__AbsoluteMove *tptz__AbsoluteMove =
      ::soap_new_set__tptz__AbsoluteMove(soap, profile_token_, ptz_vec, nullptr);
  _tptz__AbsoluteMoveResponse tptz__AbsoluteMoveResponse;
  set_credentials();
  if (proxy_ptz_.AbsoluteMove(tptz__AbsoluteMove, tptz__AbsoluteMoveResponse)) {
    std::cerr << "Error when Absolute moving" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  clog.log("Absolute move done");

  return true;
}

bool SoapThread::move_to_rel(float p, float t, float z) {
  clog.log("SoapThread::move_to_rel: Starting Relative move to p = ", p, " | t = ", t,
           " | z  = ", z);
  auto &&ptz_vec =
      ::soap_new_set_tt__PTZVector(soap, ::soap_new_set_tt__Vector2D(soap, p, t, nullptr),
                                   ::soap_new_set_tt__Vector1D(soap, z, nullptr));
  _tptz__RelativeMove *tptz__RelativeMove =
      ::soap_new_set__tptz__RelativeMove(soap, profile_token_, ptz_vec, nullptr);
  _tptz__RelativeMoveResponse tptz__RelativeMoveResponse;
  set_credentials();
  if (proxy_ptz_.RelativeMove(tptz__RelativeMove, tptz__RelativeMoveResponse)) {
    std::cerr << "Error when Relative moving" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  clog.log("SoapThread::move_to_rel Relative move done");

  return true;
}

bool SoapThread::night_mode(const IRMode &state) {
  clog.log("SoapThread::night_mode: Start switching night_mode to ", state);
  _timg__GetImagingSettings *timg__GetImagingSettings =
      ::soap_new_set__timg__GetImagingSettings(soap, video_source_token_);
  _timg__GetImagingSettingsResponse timg__GetImagingSettingsResponse;

  set_credentials();
  if (proxy_imaging_.GetImagingSettings(timg__GetImagingSettings,
                                        timg__GetImagingSettingsResponse)) {
    std::cerr << "SoapThread::night_mode: Error when retrieving Imaging Settings" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  _timg__SetImagingSettings *timg__SetImagingSettings = ::soap_new_req__timg__SetImagingSettings(
      soap, video_source_token_, timg__GetImagingSettingsResponse.ImagingSettings);

  *(timg__SetImagingSettings->ImagingSettings->IrCutFilter) =
      state == IRMode::OFF
          ? tt__IrCutFilterMode::OFF
          : state == IRMode::ON ? tt__IrCutFilterMode::ON : tt__IrCutFilterMode::AUTO;
  clog.log(
      "SoapThread::night_mode: IrCutFilter = ",
      *(timg__SetImagingSettings->ImagingSettings->IrCutFilter) == tt__IrCutFilterMode::OFF
          ? "OFF"
          : *(timg__SetImagingSettings->ImagingSettings->IrCutFilter) == tt__IrCutFilterMode::ON
                ? "ON"
                : "AUTO");
  _timg__SetImagingSettingsResponse timg__SetImagingSettingsResponse;
  set_credentials();
  if (proxy_imaging_.SetImagingSettings(timg__SetImagingSettings,
                                        timg__SetImagingSettingsResponse)) {
    std::cerr << "SoapThread::night_mode: Error when setting Imaging Settings" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  clog.log("SoapThread::night_mode: done");

  return true;
}

void SoapThread::soap_release() {
  ::soap_destroy(soap);
  ::soap_end(soap);
  ::soap_free(soap);
}

bool SoapThread::get_position(float &p, float &t, float &z) {
  clog.log("Soap: Getting Position");
  ::_tptz__GetStatus *tptz__GetStatus = ::soap_new_set__tptz__GetStatus(soap, profile_token_);
  ::_tptz__GetStatusResponse tptz__GetStatusResponse;
  set_credentials();
  if (proxy_ptz_.GetStatus(tptz__GetStatus, tptz__GetStatusResponse)) {
    std::cerr << "Unable to read current PTZ position" << std::endl;
    ::soap_stream_fault(soap, std::cerr);
    return false;
  }
  clog.log("PTZ Position: ", tptz__GetStatusResponse.PTZStatus->Position->PanTilt->x, " | ",
           tptz__GetStatusResponse.PTZStatus->Position->PanTilt->y, " | ",
           tptz__GetStatusResponse.PTZStatus->Position->Zoom->x);
  p = tptz__GetStatusResponse.PTZStatus->Position->PanTilt->x;
  t = tptz__GetStatusResponse.PTZStatus->Position->PanTilt->y;
  z = tptz__GetStatusResponse.PTZStatus->Position->Zoom->x;

  return true;
}

bool SoapThread::set_credentials() {
  ::soap_wsse_delete_Security(soap);
  return !(::soap_wsse_add_Timestamp(soap, "Time", 10) ||
           ::soap_wsse_add_UsernameTokenDigest(soap, "Auth", config.onvif_username.c_str(),
                                               config.onvif_password.c_str()));
}

void SoapThread::run() {
  thread_ = std::thread([this]() {
    do {
      clog.log("SoapThread::run: Started running");
      connected_ = init();
      error_ = !connected_;
      if (!connected_) break;
      while (true) {
        waiting_for_data_ = true;
        std::unique_lock<std::mutex> locker(mutex_);
        condition_to_consume_.wait(locker, [this]() { return !queue().empty() || exit(); });
        if (exit()) return;

        SoapAction *query;
        while (!queue_.empty()) {
          query = queue_.front();
          assert(query || !(std::cerr << "query == nullptr"));
          queue_.pop();
          if (!queue_.empty()) delete query;
        }

        waiting_for_data_ = false;
        locker.unlock();
        condition_to_queue_.notify_all();

        clog.log("SoapThread::run: processing request: ", *query);
        auto ret = query->process();
        delete query;
        if (!ret) break;
      }

    } while (0);

    soap_release();
  });
}

void SoapThread::queue(SoapAction *action) {
  assert(action || !(std::cerr << "action == null"));
  std::unique_lock<std::mutex> locker(mutex_);
  // clog.log("Waiting for empty queue or the consumer thread to finish task");
  condition_to_queue_.wait(locker,
                           [this]() { return queue_.empty() || !waiting_for_data_ || exit(); });
  if (exit()) return;
  clog.log("Adding to queue one element: ", action, " | ", *action);
  queue_.push(action);
  locker.unlock();
  condition_to_consume_.notify_all();
}

void SoapThread::must_exit() {
  exit_ = true;
  condition_to_consume_.notify_all();
  condition_to_queue_.notify_all();
}

/******************************************************************************\
 *
 *	SoapAction
 *
 \******************************************************************************/
SoapAction::SoapAction(SoapThread &soap_thread, SoapActionRunner &runner)
    : soap_thread_(soap_thread), runner_(runner) {}
SoapAction::~SoapAction() {}

SoapStopContinuousMoveAction::SoapStopContinuousMoveAction(SoapThread &soap_thread,
                                                           SoapActionRunner &runner)
    : SoapAction(soap_thread, runner) {}
bool SoapStopContinuousMoveAction::process() {
  bool ret = soap_thread().stop_move();
  runner_.stop_continuous_move_is_done(this);
  return ret;
}

std::string SoapStopContinuousMoveAction::str() const { return "SoapStopContinuousMoveAction"; }

SoapStartContinuousMoveAction::SoapStartContinuousMoveAction(SoapThread &soap_thread,
                                                             SoapActionRunner &runner, float p,
                                                             float t)
    : SoapAction(soap_thread, runner), p_(p), t_(t) {}
bool SoapStartContinuousMoveAction::process() { return soap_thread().start_move(p_, t_); }
std::string SoapStartContinuousMoveAction::str() const {
  std::stringstream ss;
  ss << "SoapStartContinuousMoveAction[" << p_ << ", " << t_ << "]";
  return ss.str();
}

SoapRelativeMoveAction::SoapRelativeMoveAction(SoapThread &soap_thread, SoapActionRunner &runner,
                                               float p, float t, float z)
    : SoapAction(soap_thread, runner), p_(p), t_(t), z_(z) {}
bool SoapRelativeMoveAction::process() {
  bool res = soap_thread().move_to_rel(p_, t_, z_);
  runner_.soap_relative_move_is_done(this);
  return res;
}
std::string SoapRelativeMoveAction::str() const {
  std::stringstream ss;
  ss << "SoapRelativeMoveAction[p = " << p_ << ", t = " << t_ << ", z = " << z_ << "]";
  return ss.str();
}

SoapAbsoluteMove::SoapAbsoluteMove(SoapThread &soap_thread, SoapActionRunner &runner)
    : SoapAction(soap_thread, runner), use_p_(false), use_t_(false), use_z_(false) {}
SoapAbsoluteMove::SoapAbsoluteMove(SoapThread &soap_thread, SoapActionRunner &runner, float p,
                                   float t, float z)
    : SoapAction(soap_thread, runner),
      p_(p),
      use_p_(true),
      t_(t),
      use_t_(true),
      z_(z),
      use_z_(true) {}
bool SoapAbsoluteMove::process() {
  float p, t, z;
  if (!use_p_ || !use_t_ || !use_z_) soap_thread().get_position(p, t, z);
  if (use_p_) p = translate_interval(p_, 0., pan_min(), 1., pan_max());
  if (use_t_) t = translate_interval(t_, 0., tilt_min(), 1., tilt_max());
  if (use_z_) z = translate_interval(z_, 0., zoom_min(), 1., zoom_max());

  return soap_thread().move_to(p, t, z);
}
std::string SoapAbsoluteMove::str() const { return "SoapAbsoluteMove"; }

SoapGetStatus::SoapGetStatus(SoapThread &soap_thread, SoapActionRunner &runner)
    : SoapAction(soap_thread, runner) {}
SoapGetStatus::~SoapGetStatus() {}
std::string SoapGetStatus::str() const {
  return "SoapGetStatus[pan = " + std::to_string(p_) + ", t = " + std::to_string(t_) +
         ", z = " + std::to_string(z_) + "]";
}
bool SoapGetStatus::process() {
  clog.log("SoapGetStatus::process");
  float p, t, z;
  soap_thread().get_position(p, t, z);
  p_ = translate_interval(p, pan_min(), 0, pan_max(), 1);
  t_ = translate_interval(t, tilt_min(), 0, tilt_max(), 1);
  z_ = translate_interval(z, zoom_min(), 0, zoom_max(), 1);
  runner_.soap_get_status_is_done(this);
  return true;
}

SoapIRModeAction::SoapIRModeAction(SoapThread &soap_thread, SoapActionRunner &runner,
                                   const IRMode &state)
    : SoapAction(soap_thread, runner), state_(state) {}
SoapIRModeAction::~SoapIRModeAction() {}
std::string SoapIRModeAction::str() const { return "SoapIRModeAction"; }
bool SoapIRModeAction::process() {
  clog.log("SoapIRModeAction::process");
  soap_thread().night_mode(state_);
  runner_.soap_night_mode_is_done(this);
  return true;
}
/******************************************************************************\
 *
 *	SoapActionRunner
 *
 \******************************************************************************/
SoapActionRunner::~SoapActionRunner() {}

SoapActionRunnerAdapter::~SoapActionRunnerAdapter() {}

void SoapActionRunnerAdapter::start_continuous_move_is_done(SoapStartContinuousMoveAction *action) {
  throw not_implemented_exception{"start_continuous_move_is_done not implemented"};
}
void SoapActionRunnerAdapter::stop_continuous_move_is_done(SoapStopContinuousMoveAction *action) {
  throw not_implemented_exception{"stop_continuous_move_is_done not implemented"};
}
void SoapActionRunnerAdapter::soap_absolute_move_is_done(SoapAbsoluteMove *action) {
  throw not_implemented_exception{"soap_absolute_move_is_done not implemented"};
}
void SoapActionRunnerAdapter::soap_get_status_is_done(SoapGetStatus *action) {
  throw not_implemented_exception{"soap_get_status_is_done not implemented"};
}
void SoapActionRunnerAdapter::soap_relative_move_is_done(SoapRelativeMoveAction *action) {
  throw not_implemented_exception{"soap_relative_move_is_done not implemented"};
}
void SoapActionRunnerAdapter::soap_night_mode_is_done(SoapIRModeAction *action) {
  throw not_implemented_exception{"soap_night_mode_is_done not implemented"};
}

void SoapActionRunnerNothing::start_continuous_move_is_done(SoapStartContinuousMoveAction *action) {
}
void SoapActionRunnerNothing::stop_continuous_move_is_done(SoapStopContinuousMoveAction *action) {}
void SoapActionRunnerNothing::soap_absolute_move_is_done(SoapAbsoluteMove *action) {}
void SoapActionRunnerNothing::soap_get_status_is_done(SoapGetStatus *action) {}
void SoapActionRunnerNothing::soap_relative_move_is_done(SoapRelativeMoveAction *action) {}
void SoapActionRunnerNothing::soap_night_mode_is_done(SoapIRModeAction *action) {}

/******************************************************************************\
 *
 *	OpenSSL
 *
 \******************************************************************************/

#ifdef WITH_OPENSSL

struct CRYPTO_dynlock_value {
  MUTEX_TYPE mutex;
};

static MUTEX_TYPE *mutex_buf;

static struct CRYPTO_dynlock_value *dyn_create_function(const char *file, int line) {
  struct CRYPTO_dynlock_value *value;
  value = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));
  if (value) MUTEX_SETUP(value->mutex);
  return value;
}

static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l, const char *file,
                              int line) {
  if (mode & CRYPTO_LOCK)
    MUTEX_LOCK(l->mutex);
  else
    MUTEX_UNLOCK(l->mutex);
}

static void dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line) {
  MUTEX_CLEANUP(l->mutex);
  free(l);
}

static void locking_function(int mode, int n, const char *file, int line) {
  if (mode & CRYPTO_LOCK)
    MUTEX_LOCK(mutex_buf[n]);
  else
    MUTEX_UNLOCK(mutex_buf[n]);
}

unsigned long id_function() { return std::hash<std::thread::id>{}(std::this_thread::get_id()); }

int CRYPTO_thread_setup() {
  int i;
  mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
  if (!mutex_buf) return SOAP_EOM;
  for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_SETUP(mutex_buf[i]);
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
  return SOAP_OK;
}

void CRYPTO_thread_cleanup() {
  int i;
  if (!mutex_buf) return;
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);
  for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_CLEANUP(mutex_buf[i]);
  free(mutex_buf);
  mutex_buf = NULL;
}

#else

/* OpenSSL not used */

int CRYPTO_thread_setup() { return SOAP_OK; }

void CRYPTO_thread_cleanup() {}

#endif

}  // namespace soap

}  // namespace app

/******************************************************************************\
 *
 *	WS-Discovery event handlers must be defined, even when not used
 *
 \******************************************************************************/

void wsdd_event_Hello(struct soap *soap, unsigned int InstanceId, const char *SequenceId,
                      unsigned int MessageNumber, const char *MessageID, const char *RelatesTo,
                      const char *EndpointReference, const char *Types, const char *Scopes,
                      const char *MatchBy, const char *XAddrs, unsigned int MetadataVersion) {}

void wsdd_event_Bye(struct soap *soap, unsigned int InstanceId, const char *SequenceId,
                    unsigned int MessageNumber, const char *MessageID, const char *RelatesTo,
                    const char *EndpointReference, const char *Types, const char *Scopes,
                    const char *MatchBy, const char *XAddrs, unsigned int *MetadataVersion) {}

soap_wsdd_mode wsdd_event_Probe(struct soap *soap, const char *MessageID, const char *ReplyTo,
                                const char *Types, const char *Scopes, const char *MatchBy,
                                struct wsdd__ProbeMatchesType *ProbeMatches) {
  return SOAP_WSDD_ADHOC;
}

void wsdd_event_ProbeMatches(struct soap *soap, unsigned int InstanceId, const char *SequenceId,
                             unsigned int MessageNumber, const char *MessageID,
                             const char *RelatesTo, struct wsdd__ProbeMatchesType *ProbeMatches) {}

soap_wsdd_mode wsdd_event_Resolve(struct soap *soap, const char *MessageID, const char *ReplyTo,
                                  const char *EndpointReference,
                                  struct wsdd__ResolveMatchType *match) {
  return SOAP_WSDD_ADHOC;
}

void wsdd_event_ResolveMatches(struct soap *soap, unsigned int InstanceId, const char *SequenceId,
                               unsigned int MessageNumber, const char *MessageID,
                               const char *RelatesTo, struct wsdd__ResolveMatchType *match) {}

int SOAP_ENV__Fault(struct soap *soap, char *faultcode, char *faultstring, char *faultactor,
                    struct SOAP_ENV__Detail *detail, struct SOAP_ENV__Code *SOAP_ENV__Code,
                    struct SOAP_ENV__Reason *SOAP_ENV__Reason, char *SOAP_ENV__Node,
                    char *SOAP_ENV__Role, struct SOAP_ENV__Detail *SOAP_ENV__Detail) {
  // populate the fault struct from the operation arguments to print it
  soap_fault(soap);
  // SOAP 1.1
  soap->fault->faultcode = faultcode;
  soap->fault->faultstring = faultstring;
  soap->fault->faultactor = faultactor;
  soap->fault->detail = detail;
  // SOAP 1.2
  soap->fault->SOAP_ENV__Code = SOAP_ENV__Code;
  soap->fault->SOAP_ENV__Reason = SOAP_ENV__Reason;
  soap->fault->SOAP_ENV__Node = SOAP_ENV__Node;
  soap->fault->SOAP_ENV__Role = SOAP_ENV__Role;
  soap->fault->SOAP_ENV__Detail = SOAP_ENV__Detail;
  // set error
  soap->error = SOAP_FAULT;
  // handle or display the fault here with soap_stream_fault(soap, std::cerr);
  // return HTTP 202 Accepted
  return soap_send_empty_response(soap, SOAP_OK);
}