#include "winheaders.h"

#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <utility>

#include "HCNetSDK.h"

#include "bgwin.h"
#include "cursors.h"
#include "main.h"
#include "synchronized_ostream.h"
#include "util.h"

#include "soap.h"

namespace app {

auto zoom_func = [](BGWindow* w, float zdelta) -> bool { return w->updateZPos(zdelta); };
auto pt_func = [](BGWindow* w, float p, float t) -> bool {
  soap::soap_thread.queue(
      new soap::SoapStartContinuousMoveAction(soap::soap_thread, soap::do_nothing_runner, p, t));
  return true;
};

auto zoom_accumulate = [](const std::queue<std::tuple<float>>& q) -> std::tuple<float> {
  float z_total = 0;
  auto queue_copy = q;
  while (!queue_copy.empty()) {
    int elem = std::get<0>(queue_copy.front());
    queue_copy.pop();
    z_total += elem;
  }

  return std::make_tuple(z_total);
};

auto pt_accumulate = [](const std::queue<std::tuple<float, float>>& q) -> std::tuple<float, float> {
  return q.back();
};

BGWindow::BGWindow()
    : m_dwnd(nullptr),
      m_onDraw(false),
      update_z_thread_(std::bind(zoom_func, this, std::placeholders::_1), zoom_accumulate),
      update_pt_thread_(std::bind(pt_func, this, std::placeholders::_1, std::placeholders::_2),
                        pt_accumulate) {}
BGWindow::~BGWindow() {}

const MainWindow* BGWindow::DrawingWindow() const { return this->m_dwnd; }

MainWindow*& BGWindow::DrawingWindow() { return this->m_dwnd; }

void BGWindow::setOnDraw(bool onDraw) {
  std::unique_lock<std::mutex> lock(this->m_lock);
  this->m_onDraw = onDraw;
}

bool BGWindow::onDraw() {
  std::unique_lock<std::mutex> lock(this->m_lock);
  return this->m_onDraw;
}

LRESULT BGWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_CREATE: {
      update_z_thread_.run();
      update_pt_thread_.run();
      return 0;
    }

    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      if (BeginPaint(this->Window(), &ps)) EndPaint(this->Window(), &ps);
      return 0;
    }

    case WM_WINDOWPOSCHANGING: {
      auto& pos = *reinterpret_cast<WINDOWPOS*>(lParam);
      if (DrawingWindow()) pos.hwndInsertAfter = m_dwnd->Window();

      return DefWindowProc(this->Window(), uMsg, wParam, lParam);
    }

    case WM_WINDOWPOSCHANGED: {
      RECT r;
      ::GetClientRect(this->Window(), &r);
      config.streamResolution = {r.right - r.left, r.bottom - r.top};

      if (this->DrawingWindow()) {
        POINT p = {0, 0};
        ::ClientToScreen(m_hwnd, &p);
        ::SetWindowPos(m_dwnd->Window(), 0, p.x, p.y, r.right - r.left, r.bottom - r.top,
                       SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
      }

      return 0;
    }

    case WM_LBUTTONDOWN: {
      setOnDraw(true);
      ::SetCapture(this->Window());
      if (onDraw() && this->DrawingWindow()) {
        start_pt_move_thread_wrapper();
      }

      return 0;
    }

    case WM_MOUSEMOVE: {
      if (onDraw()) start_pt_move_thread_wrapper();

      return 0;
    }

    case WM_LBUTTONUP: {
      if (onDraw() && this->DrawingWindow())
        ::InvalidateRgn(this->DrawingWindow()->Window(), nullptr, true);

      soap::soap_thread.queue(
          new soap::SoapStopContinuousMoveAction(soap::soap_thread, *this));
      setOnDraw(false);
      ::ReleaseCapture();

      return 0;
    }

    case WM_MOUSEWHEEL: {
      auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
      update_z_thread_.queue(zDelta);

      return 0;
    }
  }

  return DefWindowProc(this->Window(), uMsg, wParam, lParam);
}

bool BGWindow::updateZPos(int zDelta) {
  if (zDelta == 0) return true;

  clog.log("BGWindow::updatezpos: zDelta = ", zDelta);
  float d = (zDelta * config.z_sensitivity) / (20.f * WHEEL_DELTA);
  clog.log("BGWindow::updateZPos: initial dz = ", d);
  d = std::max(-1.f, std::min(1.f, d));
  clog.log("BGWindow::updateZPos: final dz = ", d);

  update_z_thread_.block_process();
  const auto action = new soap::SoapRelativeMoveAction(soap::soap_thread, *this, 0.f, 0.f, d);
  soap::soap_thread.queue(action);

  return true;
}

bool BGWindow::start_pt_move_thread_wrapper() {
  clog.log("start_pt_move_thread_wrapper: Invoked");
  RECT rect;
  ::GetClientRect(this->Window(), &rect);

  POINT p;
  ::GetCursorPos(&p);
  ::ScreenToClient(m_hwnd, &p);

  if (p.x < rect.left || p.x >= rect.right || p.y < rect.top || p.y >= rect.bottom) return false;

  clog.log("start_pt_move_thread_wrapper: position changed.");
  const auto lx = rect.right - rect.left;
  const auto ly = rect.bottom - rect.top;
  const auto dx = 2 * ((p.x - lx / 2) * 1. / lx);
  const auto dy = 2 * -((p.y - ly / 2) * 1. / ly);
  clog.log("Queueing [dp=", dx, ", dt=", dy, "]");
  soap::soap_thread.queue(
      new soap::SoapStartContinuousMoveAction(soap::soap_thread, *this, dx, dy));
  return true;
}

void BGWindow::soap_absolute_move_is_done(soap::SoapAbsoluteMove* action) {
  globalwin_->refresh_bars();
  clog.log("BGWindow::soap_absolute_move_is_done");
}

void BGWindow::soap_relative_move_is_done(soap::SoapRelativeMoveAction* action) {
  clog.log("BGWindow::soap_relative_move_is_done");
  update_z_thread_.unblock_process();
}

void BGWindow::start_continuous_move_is_done(soap::SoapStartContinuousMoveAction* action) {
  clog.log("BGWindow::start_continuous_move_is_done: start");
  clog.log("BGWindow::start_continuous_move_is_done: done");
}

void BGWindow::stop_continuous_move_is_done(soap::SoapStopContinuousMoveAction* action) {
  clog.log("BGWindow::stop_continuous_move_is_done: start");
  globalwin_->refresh_bars();
  clog.log("BGWindow::stop_continuous_move_is_done: done");
}

}  // namespace app