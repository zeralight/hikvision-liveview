#ifndef DEF_WIN_H
#define DEF_WIN_H

#include <string>
#include <random>
#include <memory>

#include "winheaders.h"

#include "basewin.h"
#include "util.h"

#include "bgwin.h"

namespace app {

class BGWindow;

class MainWindow : public BaseWindow<MainWindow>
{
    BGWindow* m_bgwin;
    
    void OnPaint();
 
  public:
    MainWindow() = default;
    ~MainWindow() = default;

    const BGWindow* BackgroundWindow() const;
    BGWindow*& BackgroundWindow();
    
    const char* ClassName() const { return "Drawing Window"; }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
};

}

// Recalculate drawing layout when the size of the window changes.

#endif