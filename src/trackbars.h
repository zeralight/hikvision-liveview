#ifndef DEF_TRACKBAR_H
#define DEF_TRACKBAR_H

#include "basewin.h"
#include "winheaders.h"

namespace app {

class GlobalWindow;

class Trackbar : public BaseWindow<Trackbar> {
  GlobalWindow *parent_;

 public:
  const char *ClassName() const { return TRACKBAR_CLASS; }
  const GlobalWindow *parent() const { return parent_; }
  GlobalWindow *&parent() { return parent_; }

  virtual BOOL Create(PCSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle = 0, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
                      int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT, HWND hWndParent = 0, HMENU hMenu = 0,
                      HCURSOR hCursor = nullptr);

  virtual LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
};

class Panbar : public Trackbar {};

class Tiltbar : public Trackbar {};

class Zoombar : public Trackbar {};

}  // namespace app

#endif