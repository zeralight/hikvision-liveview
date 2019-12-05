#include "globalwin.h"
#include "winheaders.h"

#include "synchronized_ostream.h"

#include "cursors.h"
#include "main.h"

#include <cstring>
#include <filesystem>
#include <iostream>

namespace app {
LRESULT GlobalWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  static bool first_time = true;
  switch (message) {
    case WM_CREATE: {
      zbar_->parent() = this;
      if (!zbar_->Create("Zoom bar",
                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE | TBS_VERT, 0,
                         0, 0, 0, 0, this->Window())) {
        std::cerr << "Creating Z trackbar: " << winErrorStr(::GetLastError()) << '\n';
        return 1;
      }

      vbar_->parent() = this;
      if (!vbar_->Create("Volume bar",
                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE | TBS_VERT, 0,
                         0, 0, 0, 0, this->Window())) {
        std::cerr << "GlobalWindow: Creating Voume trackbar: " << winErrorStr(::GetLastError())
                  << '\n';
        return 1;
      }

      pbar_->parent() = this;
      if (!pbar_->Create("Pan bar", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, 0,
                         this->Window())) {
        std::cerr << "GlobalWindow: Creating pbar: " << winErrorStr(::GetLastError()) << '\n';
        return 1;
      }

      tbar_->parent() = this;
      if (!tbar_->Create("Tilt bar", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, 0,
                         this->Window())) {
        std::cerr << "GlobalWindow: Creating Tbar: " << winErrorStr(::GetLastError()) << '\n';
        return 1;
      }

      record_button_->parent() = this;
      if (!record_button_->Create("Start Recording", WS_CHILD | WS_VISIBLE | BS_BITMAP, 0, 0, 0, 0,
                                  0, this->Window())) {
        std::cerr << "GlobalWindow: Creating Tbar: " << winErrorStr(::GetLastError()) << '\n';
        return 1;
      }
      ::SendMessage(record_button_->Window(), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)record_on_bmp_);

      // light_button_->parent() = this;
      // if (!light_button_->Create("Light", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, 0,
      // this->Window())) {
      //   std::cerr << "GlobalWindow: Creating Tbar: " << winErrorStr(::GetLastError()) << '\n';
      //   return 1;
      // }

      // ::NET_DVR_VOLUME_CFG net_dvr_volume_cfg;
      // DWORD bytes_returned;
      // if (!::NET_DVR_GetDVRConfig(config.uid[0], NET_DVR_GET_AUDIOIN_VOLUME_CFG, 1,
      // &net_dvr_volume_cfg, sizeof net_dvr_volume_cfg, &bytes_returned)) {
      //   std::cerr << "Reading NET_DVR_GET_AUDIOIN_VOLUME_CFG Failed. " << ::NET_DVR_GetErrorMsg()
      //   << '\n';
      // }
      // clog.log("GlobalWindow::HandleMessage:WM_CREATE: input volume = ",
      // net_dvr_volume_cfg.wVolume[1]);
    } break;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(this->Window(), &ps);
      FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
      EndPaint(this->Window(), &ps);
    } break;

    case WM_WINDOWPOSCHANGED: {
      RECT r;
      ::GetClientRect(this->Window(), &r);
      const auto w = r.right - r.left;
      const auto h = r.bottom - r.top;
      const auto bar_thickness = 24;
      const auto button_w = 24;
      const auto button_h = 24;
      const auto bgwin_w = w - button_w - 4;
      const auto bgwin_h = h - bar_thickness - 30;
      const auto record_button_x = r.left + bgwin_w + 1;
      const auto record_button_y = r.top + h / 8 + 5;
      const auto vbar_x = record_button_x;
      const auto vbar_y = record_button_y + button_h + 10;
      const auto vbar_w = bar_thickness;
      const auto vbar_h = h / 4;
      const auto pbar_x = r.left + w / 4;
      const auto pbar_y = r.top + bgwin_h + 5;
      const auto pbar_w = w / 4;
      const auto pbar_h = bar_thickness;
      const auto tbar_x = pbar_x + pbar_w + 10;
      const auto tbar_y = pbar_y;
      const auto tbar_w = w / 4;
      const auto tbar_h = bar_thickness;
      const auto zbar_x = vbar_x;
      const auto zbar_y = vbar_y + vbar_h + 10;
      const auto zbar_w = bar_thickness;
      const auto zbar_h = h / 4;
      const auto flags = SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME;
      ::SetWindowPos(bgwin_->Window(), 0, 0, 0, bgwin_w, bgwin_h, flags);
      ::SetWindowPos(zbar_->Window(), 0, zbar_x, zbar_y, zbar_w, zbar_h, flags);
      ::SetWindowPos(pbar_->Window(), 0, pbar_x, pbar_y, pbar_w, pbar_h, flags);
      ::SetWindowPos(tbar_->Window(), 0, tbar_x, tbar_y, tbar_w, tbar_h, flags);
      if (first_time) {
        first_time = false;
        refresh_bars();
      }
      if (!::SetWindowPos(record_button_->Window(), 0, record_button_x, record_button_y, button_w,
                          button_h, flags)) {
        std::cerr << "GlobalWindow:: Repositioning Record Button: " << winErrorStr(::GetLastError())
                  << '\n';
        return 1;
      }
      if (!::SetWindowPos(vbar_->Window(), 0, vbar_x, vbar_y, vbar_w, vbar_h, flags)) {
        std::cerr << "GlobalWindow:: Repositioning Volume trackbar: "
                  << winErrorStr(::GetLastError()) << '\n';
        return 1;
      }
    } break;

    case WM_HSCROLL: {
      const auto h = (HWND)lParam;
      if (h == tbar_->Window() || h == pbar_->Window()) {
        const auto pos = ::SendMessage(h, TBM_GETPOS, 0, 0);
        const auto min = ::SendMessage(h, TBM_GETRANGEMIN, 0, 0);
        const auto max = ::SendMessage(h, TBM_GETRANGEMAX, 0, 0);
        const float d = (pos - min) * 1.f / (max - min);
        const auto action = new soap::SoapAbsoluteMove(soap::soap_thread, *this);
        if (h == pbar_->Window()) {
          clog.log("GlobalWindow: Panbar Scroll:", pos);
          clog.log("GlobalWindow: Pan move to:", d);
          action->setPan(d);
        } else {
          clog.log("GlobalWindow: Tiltbar Scroll:", pos);
          clog.log("GlobalWindow: Tilt move to:", d);
          action->setTilt(d);
        }
        soap::soap_thread.queue(action);
      }
    } break;

    case WM_VSCROLL: {
      const auto h = (HWND)lParam;
      if (h == zbar_->Window() || h == vbar_->Window()) {
        const auto pos = ::SendMessage(h, TBM_GETPOS, 0, 0);
        const auto min = ::SendMessage(h, TBM_GETRANGEMIN, 0, 0);
        const auto max = ::SendMessage(h, TBM_GETRANGEMAX, 0, 0);
        if (h == zbar_->Window()) {
          const float d = (pos - min) * 1.f / (max - min);
          const auto action = new soap::SoapAbsoluteMove(soap::soap_thread, *this);
          action->setZoom(d);
          clog.log("GlobalWindow: Zoombar Scroll:", pos);
          soap::soap_thread.queue(action);
        } else {
          clog.log("GlobalWindow::HandleMessage:VM_SCROLL: volume scroll");
          /* const float d = (pos - min) * 1.f / (max - min);
          const auto action =
              new soap::SoapRelativeMoveAction(soap::soap_thread, *this, 0.f, d, 0.f);
          soap::soap_thread.queue(action); */
        }
      }
    } break;

    case WM_COMMAND: {
      const auto h = (HWND)lParam;
      if (h == record_button_->Window() && HIWORD(wParam) == BN_CLICKED) {
        auto recording_tmp = !recording_;
        std::filesystem::path path(config.record_dir);
        try {
          std::filesystem::create_directories(path);
        } catch (const std::filesystem::filesystem_error& e) {
          std::cerr << "Error when creating directory " << config.record_dir << ": " << e.what()
                    << '\n';
          return 1;
        }
        path /= get_current_time() + ".mp4";
        const auto path_str = path.string();
        std::shared_ptr<char[]> pfname(new char[path_str.size() + 1]);
        std::strcpy(pfname.get(), path_str.c_str());
        if (recording_tmp && !::NET_DVR_SaveRealData(config.real_play_handle, pfname.get())) {
          std::cerr << "Error when starting recording: " << ::NET_DVR_GetErrorMsg() << '\n';
          return 1;
        } else if (!recording_tmp && !::NET_DVR_StopSaveRealData(config.real_play_handle)) {
          std::cerr << "Error when stopping recording: " << ::NET_DVR_GetErrorMsg() << '\n';
          return 1;
        }
        recording_ = recording_tmp;
        if (recording_) {
          clog.log("Started recording to ", pfname.get());
          ::SendMessage(record_button_->Window(), BM_SETIMAGE, IMAGE_BITMAP,
                        (LPARAM)record_off_bmp_);
        } else {
          clog.log("Stopped recording to ", pfname.get());
          ::SendMessage(record_button_->Window(), BM_SETIMAGE, IMAGE_BITMAP,
                        (LPARAM)record_on_bmp_);
        }
      }
    }; break;

    case WM_KEYUP: {
      wchar_t c = (wchar_t)wParam;
      clog.log("Pressed key: [", c, "]");
      if ((c >= 48 and c < 48 + 9) || (c >= 96 and c < 96 + 10)) {
        int d = c - (c < 96 ? 48 : 96);
        clog.log("digit Pressed: ", d);
        if (keyboard_state_.size() > 0)
          keyboard_state_ += '0' + (char)d;
        else
          keyboard_state_ = "";
      } else if (c == 'P') {
        clog.log("P Pressed");
        keyboard_state_ = "P";
      } else if (c == 'T') {
        clog.log("T Pressed");
        keyboard_state_ = "T";
      } else if (c == 'Z') {
        clog.log("Z Pressed");
        keyboard_state_ = "Z";
      } else if (::GetKeyState('-') & 0x8000) {
        clog.log("- Pressed");
        if (keyboard_state_ == "P" || keyboard_state_ == "T" || keyboard_state_ == "Z")
          keyboard_state_ += '-';
        else
          keyboard_state_ = "";
      } else if (c == 13 or c == 32) {
        clog.log("Enter|Space Pressed");
        if (keyboard_state_.size() > 1 && keyboard_state_[keyboard_state_.size()-1] != '-') {
          clog.log("Validating keyboard input: ", keyboard_state_); // TODO on going
        } else {
          keyboard_state_ = "";
        }
      }
    }; break;

    case WM_DESTROY: {
      PostQuitMessage(0);
    } break;

    default: { return ::DefWindowProc(this->Window(), message, wParam, lParam); } break;
  }

  return 0;
}

void GlobalWindow::refresh_bars() {
  clog.log("GlobalWindow::refresh_bars: Queueing a new action");
  soap::SoapAction* action = new soap::SoapGetStatus(soap::soap_thread, *this);
  soap::soap_thread.queue(action);
  clog.log("GlobalWindow::refresh_bars: Queueing done");
}

void GlobalWindow::soap_get_status_is_done(soap::SoapGetStatus* action) {
  clog.log("GlobalWindow::soap_get_status_is_done");

  clog.log("GlobalWindow::soap_get_status_is_done: action = ", *action);
  auto min = ::SendMessage(zbar_->Window(), TBM_GETRANGEMIN, 0, 0);
  auto max = ::SendMessage(zbar_->Window(), TBM_GETRANGEMAX, 0, 0);
  auto val = translate_interval(action->zoom(), 0, min, 1, max);
  clog.log("zoom (min, max) =(", min, max, ")");
  clog.log("GlobalWindow::soap_get_status_is_done: z val = ", val);
  ::SendMessage(zbar_->Window(), TBM_SETPOS, true, (LPARAM)val);

  min = ::SendMessage(pbar_->Window(), TBM_GETRANGEMIN, 0, 0);
  max = ::SendMessage(pbar_->Window(), TBM_GETRANGEMAX, 0, 0);
  val = translate_interval(action->pan(), 0, min, 1, max);
  clog.log("pan (min, max) =(", min, max, ")");
  clog.log("GlobalWindow::soap_get_status_is_done: pan val = ", val);
  ::SendMessage(pbar_->Window(), TBM_SETPOS, true, (LPARAM)val);

  min = ::SendMessage(tbar_->Window(), TBM_GETRANGEMIN, 0, 0);
  max = ::SendMessage(tbar_->Window(), TBM_GETRANGEMAX, 0, 0);
  clog.log("tilt (min, max) =(", min, max, ")");
  val = translate_interval(action->tilt(), 0, min, 1, max);
  clog.log("GlobalWindow::soap_get_status_is_done: tilt val = ", val);
  ::SendMessage(tbar_->Window(), TBM_SETPOS, true, (LPARAM)val);
}

void GlobalWindow::soap_relative_move_is_done(soap::SoapRelativeMoveAction* action) {
  clog.log("GlobalWindow::relative_move_is_done");
}

}  // namespace app