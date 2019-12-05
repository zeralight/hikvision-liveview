#ifndef DEF_BGWIN_H
#define DEF_BGWIN_H

#include "winheaders.h"

#include <cstdlib>
#include <functional>
#include <mutex>
#include <queue>

#include "Consumer.h"
#include "basewin.h"
#include "globalwin.h"
#include "soap.h"
#include "util.h"
#include "win.h"

namespace app {

class MainWindow;
class GlobalWindow;

class BGWindow : public BaseWindow<BGWindow>, public soap::SoapActionRunnerAdapter {
  GlobalWindow* globalwin_;
  MainWindow* m_dwnd;
  bool m_onDraw;
  std::mutex m_lock;
  Consumer<std::function<bool(float)>, std::function<std::tuple<float>(const std::queue<std::tuple<float>>&)>, float> update_z_thread_;
  Consumer<std::function<bool(float, float)>, std::function<std::tuple<float, float>(const std::queue<std::tuple<float, float>>&)>, float,
           float>
      update_pt_thread_;

  void setOnDraw(bool onDraw);

  bool start_pt_move_thread_wrapper();

 public:
  BGWindow();
  ~BGWindow();

  const char* ClassName() const { return "Background Window"; }

  bool onDraw();

  const MainWindow* DrawingWindow() const;
  MainWindow*& DrawingWindow();

  const GlobalWindow* globlawin() const { return globalwin_; }
  GlobalWindow*& globalwin() { return globalwin_; }

  bool stopPTMove();

  bool updateZPos(int zDelta);

  LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

  virtual void soap_absolute_move_is_done(soap::SoapAbsoluteMove* action) override;
  virtual void soap_relative_move_is_done(soap::SoapRelativeMoveAction* action) override;
  virtual void start_continuous_move_is_done(soap::SoapStartContinuousMoveAction* action) override;
  virtual void stop_continuous_move_is_done(soap::SoapStopContinuousMoveAction* action) override;
};

}  // namespace app

#endif