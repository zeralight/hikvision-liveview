#ifndef DEF_BUTTON_H
#define DEF_BUTTON_H

#include "basewin.h"
#include "winheaders.h"

namespace app {

class GlobalWindow;

class Button : public BaseWindow<Button> {
 protected:
  GlobalWindow* parent_;

 public:
  Button() = default;
  virtual ~Button() = default;
  
  const char* ClassName() const { return "BUTTON"; }

  GlobalWindow*& parent() { return parent_; }
  const GlobalWindow* parent() const { return parent_; }

  virtual BOOL Create(PCSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle = 0, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
                      int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT, HWND hWndParent = 0, HMENU hMenu = 0,
                      HCURSOR hCursor = nullptr) {
    m_hwnd = CreateWindowEx(dwExStyle, ClassName(), lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, GetModuleHandle(NULL),
                            this);
    return (m_hwnd ? TRUE : FALSE);
  }

  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
      case WM_DESTROY: {
        PostQuitMessage(0);
      } break;

      default: { return ::DefWindowProc(this->Window(), message, wParam, lParam); } break;
    }

    return 0;
  }
};

}  // namespace app

#endif