#pragma once
// Look of the app: light theme with a red accent (Scalpellino is dark with blue).
// The accent is for UI chrome only: buttons, tabs, active tool, focused fields,
// selected list rows. Views keep their own colours (render/view_draw.hpp).
#include "imgui.h"

namespace mazz {

void applyTheme();

namespace theme {
constexpr ImVec4 kAccent{0.82f, 0.20f, 0.24f, 1.0f};
constexpr ImVec4 kAccentHover{0.90f, 0.30f, 0.33f, 1.0f};
constexpr ImVec4 kAccentActive{0.70f, 0.14f, 0.18f, 1.0f};
constexpr ImVec4 kAccentSoft{0.82f, 0.20f, 0.24f, 0.22f};   // selected rows, headers
constexpr ImVec4 kAccentSoftHover{0.82f, 0.20f, 0.24f, 0.32f};
constexpr ImVec4 kTextDim{0.45f, 0.46f, 0.50f, 1.0f};
} // namespace theme

} // namespace mazz
