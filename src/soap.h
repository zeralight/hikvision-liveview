#ifndef DEF_SOAP_H
#define DEF_SOAP_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <string>

#include <queue>

#include "soap/soapDeviceBindingProxy.h"
#include "soap/soapH.h"
#include "soap/soapMediaBindingProxy.h"
#include "soap/soapPTZBindingProxy.h"
#include "soap/soapImagingBindingProxy.h"

namespace app {

namespace soap {

int CRYPTO_thread_setup();
void CRYPTO_thread_cleanup();

class SoapAction;
class SoapActionRunner;

enum class IRMode {
  ON,
  OFF,
  AUTO
};
std::ostream& operator<<(std::ostream& out, const IRMode& state);

class SoapThread {
  std::mutex mutex_;
  std::condition_variable condition_to_consume_;
  std::condition_variable condition_to_queue_;
  std::queue<SoapAction*> queue_;
  std::queue<SoapAction*> processed_queue_;
  bool waiting_for_data_;
  std::atomic<bool> connected_;
  std::atomic<bool> error_;
  std::atomic<bool> exit_;
  std::thread thread_;

  std::string onvif_username;
  std::string onvif_password;
  // uint16_t soap_port;
  std::string soap_endpoint;
  struct soap* soap;
  DeviceBindingProxy proxy_device_;
  MediaBindingProxy proxy_media_;
  PTZBindingProxy proxy_ptz_;
  ImagingBindingProxy proxy_imaging_;
  ::tt__Profile* soap_profile_;
  ::tt__PTZConfiguration* soap_ptz_configuration_;
  tt__ReferenceToken profile_token_;
  tt__ReferenceToken video_source_token_;
  tt__ReferenceToken audio_source_token_;
  tt__ReferenceToken audio_main_output_token_;

  // int main_audio_output_level_min_;
  // int main_audio_output_level_max_;

  bool init();
  bool set_credentials();
  void soap_release();

  const float& pan_max() const { return soap_ptz_configuration_->PanTiltLimits->Range->XRange->Max; }
  const float& pan_min() const { return soap_ptz_configuration_->PanTiltLimits->Range->XRange->Min; }

  const float& tilt_max() const { return soap_ptz_configuration_->PanTiltLimits->Range->YRange->Max; }
  const float& tilt_min() const { return soap_ptz_configuration_->PanTiltLimits->Range->YRange->Min; }

  const float& zoom_max() const { return soap_ptz_configuration_->ZoomLimits->Range->XRange->Max; }
  const float& zoom_min() const { return soap_ptz_configuration_->ZoomLimits->Range->XRange->Min; }

  friend class SoapAction;

 public:
  SoapThread();

  const std::mutex& mutex() const { return mutex_; }
  std::mutex& mutex() { return mutex_; }

  const std::queue<SoapAction*>& queue() const { return queue_; }
  std::queue<SoapAction*>& queue() { return queue_; }

  const std::atomic<bool>& connected() const { return connected_; }

  const std::atomic<bool>& error() const { return error_; }

  const std::atomic<bool>& exit() const { return exit_; }
  std::atomic<bool>& exit() { return exit_; }

  const std::thread& thread() const { return thread_; }
  std::thread& thread() { return thread_; }

  void run();

  void queue(SoapAction* action);

  void must_exit();

  bool get_position(float& p, float& t, float& z);

  bool start_move(float p, float t);

  bool stop_move();

  bool move_to(float p, float t, float z);

  bool move_to_rel(float p, float t, float z);

  bool night_mode(const IRMode& state);
};

extern SoapThread soap_thread;

class SoapAction {
  SoapThread& soap_thread_;

 protected:
  SoapActionRunner& runner_;
  SoapAction(SoapThread& soap_thread, SoapActionRunner& runner_);

 public:
  virtual ~SoapAction() = 0;

  const SoapThread& soap_thread() const { return soap_thread_; }
  SoapThread& soap_thread() { return soap_thread_; }

  const SoapActionRunner& soap_action_runner() const { return runner_; }
  SoapActionRunner& soap_action_runner() { return runner_; }

  const float& pan_max() const { return soap_thread_.pan_max(); }
  const float& pan_min() const { return soap_thread_.pan_min(); }
  const float& tilt_max() const { return soap_thread_.tilt_max(); }
  const float& tilt_min() const { return soap_thread_.tilt_min(); }
  const float& zoom_max() const { return soap_thread_.zoom_max(); }
  const float& zoom_min() const { return soap_thread_.zoom_min(); }

  virtual bool process() = 0;
  virtual std::string str() const = 0;
  friend std::ostream& operator<<(std::ostream& out, const SoapAction& action) { return (out << action.str()); }
};

class SoapStopContinuousMoveAction : public SoapAction {
 public:
  SoapStopContinuousMoveAction(SoapThread& soap_thread, SoapActionRunner& runner);
  virtual ~SoapStopContinuousMoveAction() = default;
  virtual bool process();
  virtual std::string str() const override;
};

class SoapStartContinuousMoveAction : public SoapAction {
  float p_;
  float t_;

 public:
  SoapStartContinuousMoveAction(SoapThread& soap_thread, SoapActionRunner& runner, float p, float t);
  virtual ~SoapStartContinuousMoveAction() = default;
  virtual bool process();
  virtual std::string str() const override;
};

class SoapRelativeMoveAction : public SoapAction {
  float p_;
  float t_;
  float z_;
  
 public:
  SoapRelativeMoveAction(SoapThread& soap_thread, SoapActionRunner& runner, float t, float p, float z);
  virtual ~SoapRelativeMoveAction() = default;
  virtual bool process();
  virtual std::string str() const override;
};

class SoapAbsoluteMove : public SoapAction {
  float p_;

  bool use_p_;
  float t_;
  bool use_t_;
  float z_;
  bool use_z_;

 public:
  SoapAbsoluteMove(SoapThread& soap_thread, SoapActionRunner& runner);
  SoapAbsoluteMove(SoapThread& soap_thread, SoapActionRunner& runner, float p, float t, float z);
  virtual ~SoapAbsoluteMove() = default;

  bool has_pan() const { return use_p_; }
  float pan() const { return p_; }
  SoapAbsoluteMove& setPan(float p) {
    p_ = p;
    use_p_ = true;
    return *this;
  }

  bool has_tilt() const { return use_t_; }
  float tilt() const { return t_; }
  SoapAbsoluteMove& setTilt(float t) {
    t_ = t;
    use_t_ = true;
    return *this;
  }

  bool has_zoom() const { return use_z_; }
  float zoom() const { return z_; }
  SoapAbsoluteMove& setZoom(float z) {
    z_ = z;
    use_z_ = true;
    return *this;
  }

  void clear() { use_p_ = use_t_ = use_z_ = false; }

  virtual std::string str() const override;
  virtual bool process();
};

class SoapGetStatus : public SoapAction {
 private:
  float p_;
  float t_;
  float z_;

 public:
  SoapGetStatus(SoapThread& soap_thread, SoapActionRunner& runner);
  ~SoapGetStatus();
  float pan() const { return p_; }
  float tilt() const { return t_; }
  float zoom() const { return z_; }
  virtual std::string str() const override;
  bool process() override;
};

class SoapIRModeAction: public SoapAction {
  private:
  IRMode state_;
  public:
  SoapIRModeAction(SoapThread& soap_thread, SoapActionRunner& runner, const IRMode& state);
  ~SoapIRModeAction();
  IRMode state() const { return state_; }
  virtual std::string str() const override;
  bool process() override;
};

class SoapActionRunner {
 protected:
  SoapActionRunner() = default;

 public:
  virtual void start_continuous_move_is_done(SoapStartContinuousMoveAction* action) = 0;
  virtual void stop_continuous_move_is_done(SoapStopContinuousMoveAction* action) = 0;
  virtual void soap_absolute_move_is_done(SoapAbsoluteMove* action) = 0;
  virtual void soap_get_status_is_done(SoapGetStatus* action) = 0;
  virtual void soap_relative_move_is_done(SoapRelativeMoveAction* action) = 0;
  virtual void soap_night_mode_is_done(SoapIRModeAction* action) = 0;
  virtual ~SoapActionRunner() = 0;
};

class SoapActionRunnerAdapter : public SoapActionRunner {
 public:
  SoapActionRunnerAdapter() = default;
  virtual ~SoapActionRunnerAdapter();
  virtual void start_continuous_move_is_done(SoapStartContinuousMoveAction* action) override;
  virtual void stop_continuous_move_is_done(SoapStopContinuousMoveAction* action) override;
  virtual void soap_absolute_move_is_done(SoapAbsoluteMove* action) override;
  virtual void soap_get_status_is_done(SoapGetStatus* action) override;
  virtual void soap_relative_move_is_done(SoapRelativeMoveAction* action) override;
  virtual void soap_night_mode_is_done(SoapIRModeAction* action) override;
};

class SoapActionRunnerNothing : public SoapActionRunner {
 public:
  SoapActionRunnerNothing() = default;
  ~SoapActionRunnerNothing() = default;
  void start_continuous_move_is_done(SoapStartContinuousMoveAction* action) override final;
  void stop_continuous_move_is_done(SoapStopContinuousMoveAction* action) override final;
  void soap_absolute_move_is_done(SoapAbsoluteMove* action) override final;
  void soap_get_status_is_done(SoapGetStatus* action) override final;
  void soap_relative_move_is_done(SoapRelativeMoveAction* action) override final;
  virtual void soap_night_mode_is_done(SoapIRModeAction* action) override;
};

extern SoapActionRunnerNothing do_nothing_runner;

}  // namespace soap
}  // namespace app
#endif
