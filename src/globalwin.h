#ifndef DEF_GLOBALWIN_H
#define DEF_GLOBALWIN_H

#include <memory>

#include "winheaders.h"

#include "basewin.h"
#include "bgwin.h"
#include "button.h"
#include "cursors.h"
#include "soap.h"
#include "trackbars.h"

namespace app {

class BGWindow;

class GlobalWindow : public BaseWindow<GlobalWindow>, public soap::SoapActionRunnerAdapter {
  BGWindow *bgwin_;
  std::unique_ptr<Trackbar> zbar_;
  std::unique_ptr<Trackbar> pbar_;
  std::unique_ptr<Trackbar> tbar_;
  std::unique_ptr<Trackbar> vbar_;
  std::unique_ptr<Button> record_button_;
  bool recording_;
  HBITMAP record_on_bmp_;
  HBITMAP record_off_bmp_;
  boolean visible_ = false;
  std::string keyboard_state_ = "";

 public:
  GlobalWindow()
      : bgwin_(nullptr),
        zbar_(new Zoombar()),
        pbar_(new Panbar()),
        tbar_(new Tiltbar()),
        vbar_(new Trackbar()),
        record_button_(new Button()),
        recording_(false) {
    record_on_bmp_ = mat2bitmap(start_record_matrix);
    record_off_bmp_ = mat2bitmap(stop_record_matrix);
  }
  ~GlobalWindow() = default;

  const char *ClassName() const { return "Global Window"; }

  const BGWindow *bgwin() const { return bgwin_; }
  BGWindow *&bgwin() { return bgwin_; }

  const std::unique_ptr<Trackbar> &zbar() const { return zbar_; }
  std::unique_ptr<Trackbar> &zbar() { return zbar_; }

  const std::unique_ptr<Trackbar> &pbar() const { return pbar_; }
  std::unique_ptr<Trackbar> &pbar() { return pbar_; }

  const std::unique_ptr<Trackbar> &tbar() const { return tbar_; }
  std::unique_ptr<Trackbar> &tbar() { return tbar_; }

  const std::unique_ptr<Trackbar> &vbar() const { return vbar_; }
  std::unique_ptr<Trackbar> &vbar() { return vbar_; }

  const std::unique_ptr<Button> &record_button() const { return record_button_; }
  std::unique_ptr<Button> &record_button() { return record_button_; }

  boolean& visible() { return visible_; }
  boolean visible() const { return visible_; }
  
  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

  void refresh_bars();

  virtual void soap_get_status_is_done(soap::SoapGetStatus *action) override;
  virtual void soap_relative_move_is_done(soap::SoapRelativeMoveAction *action) override;
};

}  // namespace app

#endif