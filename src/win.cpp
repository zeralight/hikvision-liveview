#include "winheaders.h"
#include "win.h"

#include <iostream>
#include <memory>

#include "util.h"
#include "synchronized_ostream.h"
#include "cursors.h"

namespace app {

const BGWindow* MainWindow::BackgroundWindow() const { return this->m_bgwin; }
BGWindow*& MainWindow::BackgroundWindow() { return this->m_bgwin; }

void MainWindow::OnPaint()
{
    PAINTSTRUCT ps;
	HDC hdc = BeginPaint(this->Window(), &ps);

    HDC memDC = ::CreateCompatibleDC(hdc);
    RECT rc;
    ::GetClientRect(this->Window(), &rc);
    HBITMAP bmp = ::CreateCompatibleBitmap(hdc, rc.right-rc.left, rc.bottom-rc.top);
    HBITMAP oldmap = (HBITMAP) ::SelectObject(memDC, bmp);

    Gdiplus::Graphics graphics(memDC);
    graphics.Clear(Gdiplus::Color::White);

    auto x_start = (INT) (rc.left + (rc.right-rc.left)/2 - center_cross_matrix.size()/2);
    auto y_start = (INT) (rc.top + (rc.bottom-rc.top)/2 - center_cross_matrix[0].size()/2);

    Gdiplus::SolidBrush brush(Gdiplus::Color::Green);
    for (size_t i = 0; i < center_cross_matrix.size(); ++i)
    {
        for (size_t j = 0; j < center_cross_matrix[0].size(); ++j)
        {
            uint8_t r, g, b;
            std::tie(r, g, b) = center_cross_matrix[i][j];
            // if (r != 0 || g != 0 || b != 0)
                ::SetPixel(memDC, x_start+j, y_start+i, RGB(r, g, b));
        }
    }
    ::BitBlt(hdc, 0, 0, rc.right-rc.left, rc.bottom-rc.top, memDC, 0, 0, SRCCOPY);
    ::SelectObject(memDC, oldmap);
    ::DeleteObject(bmp);
    ::DeleteDC(memDC);

	::EndPaint(this->Window(), &ps);
}

LRESULT CALLBACK MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{       
		case WM_ERASEBKGND:
		{
			return true;
		}

        case WM_PAINT:
        {
            OnPaint();
        }
        break;
        
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
        break;

        default:
        {
            return ::DefWindowProc(this->Window(), message, wParam, lParam);
        }
        break;
	}

	return 0;
}

}