#ifndef DEF_CURSORS_H
#define DEF_CURSORS_H

#include <array>
#include <cstdint>
#include <tuple>
#include <utility>

#include "winheaders.h"

namespace app {

template <size_t N1, size_t N2, typename Tuple>
HBITMAP mat2bitmap(std::array<std::array<Tuple, N1>, N2>& mat) {
  HDC maindc = ::GetDC(NULL);
  HDC tempdc = ::CreateCompatibleDC(maindc);
  HBITMAP temp_bitmap = ::CreateCompatibleBitmap(maindc, N2, N1);
  HBITMAP old_bitmap = (HBITMAP)::SelectObject(tempdc, temp_bitmap);

  for (size_t i = 0; i < N1; ++i) {
    for (size_t j = 0; j < N2; ++j) {
      const auto& rgba{mat[i][j]};
      ::SetPixel(tempdc, i, j, RGB(std::get<0>(rgba), std::get<1>(rgba), std::get<2>(rgba)));
    }
  }

  ::SelectObject(tempdc, old_bitmap);

  ::DeleteDC(tempdc);
  ::ReleaseDC(NULL, maindc);

  return temp_bitmap;
}

HCURSOR bitmap2cursor(HBITMAP hSourceBitmap, COLORREF clrTransparent, DWORD xHotspot, DWORD yHotspot);

extern std::array<std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 16>, 16> cursor_matrix;
extern std::array<std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 16>, 16> center_cross_matrix;
extern std::array<std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 24>, 24> start_record_matrix;
extern std::array<std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 24>, 24> stop_record_matrix;
extern std::array<std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 24>, 24> sound_full_matrix;


}  // namespace app

#endif