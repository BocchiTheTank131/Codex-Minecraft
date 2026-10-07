#pragma once
#include "UiLayout.h"
#include <array>
#include <algorithm>

namespace MenuLayout {
inline constexpr std::array<int, 12> VideoActions{0, 1, 2, 5, 6, 7, 8, 13, 11, 12, 9, 10};
inline UiRect panel(int w, int h) { return {w * .5f - 310, h * .5f - 310, 620, 620}; }
inline UiRect tab(int index, int w, int h) { const auto p = panel(w,h); return {p.x + 20 + index * 195.0f, p.y + 58, 190, 35}; }
inline UiRect videoRow(int index, int w, int h) { const auto p = panel(w,h); return {p.x + 24, p.y + 108 + index * 35.0f, 572, 32}; }
inline UiRect back(int w, int h) { const auto p = panel(w,h); return {p.x + 190, p.y + 565, 240, 38}; }
inline UiRect hubRow(int index, int w, int h) { const auto p = panel(w,h); return {p.x + 120, p.y + 175 + index * 75.0f, 380, 54}; }
inline UiRect worldRow(int index, int w, int h) { const auto p = panel(w,h); return {p.x + 24, p.y + 92 + index * 62.0f, 572, 56}; }
inline UiRect worldAction(int index, int w, int h) { const auto p=panel(w,h); return {p.x + 24 + index * 194.0f, p.y + 552, 184, 44}; }
inline UiRect pageButton(int index, int w, int h) { const auto p=panel(w,h); return {p.x + 24 + index * 490.0f, p.y + 478, 82, 32}; }
inline UiRect creationField(int index, int w, int h) { const auto p=panel(w,h); return {p.x+40,p.y+135+index*105.0f,540,52}; }
inline UiRect creationButton(int index, int w, int h) { const auto p=panel(w,h); return {p.x+40+index*280.0f,p.y+535,260,48}; }
inline UiRect sensitivity(int w, int h) { return {w*.5f-185,h*.5f+170,370,36}; }
}
