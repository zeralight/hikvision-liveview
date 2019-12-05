#include "trackbars.h"

// #include "synchronized_ostream.h"

namespace app {
BOOL Trackbar::Create(PCSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent,
                      HMENU hMenu, HCURSOR hCursor) {
  m_hwnd = ::CreateWindowEx(dwExStyle, ClassName(), lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, GetModuleHandle(NULL),
                            this);
  return (m_hwnd ? TRUE : FALSE);
}
LRESULT Trackbar::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  return ::DefWindowProc(this->Window(), message, wParam, lParam);
}
}  // namespace app