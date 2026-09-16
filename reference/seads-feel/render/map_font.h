#pragma once

// MAP FONT (S-mapread, Chad 2026-08-09: "Use arial font"). One shared,
// lazily-loaded Arial face for BOTH map layers (render/tourist_map.cpp's
// basemap labels and render/draw.cpp's tactical/corner text) so the map reads
// as one printed sheet.
//
// PORTABILITY TRADE (flagged for Chad): Arial ships with Windows but is NOT
// redistributable, so it is loaded from C:/Windows/Fonts at runtime rather
// than vendored into assets/. If the file is missing (a non-Windows box, or a
// stripped install) this falls back to Arial's bold face and then to raylib's
// built-in default — the map still draws, just in the old face. This mirrors
// the EXACT pattern already shipped for the nose-art tag font in
// render/draw.cpp. Swapping in a redistributable sans (e.g. Liberation Sans,
// metric-compatible with Arial) under assets/ is a one-line change here.
//
// Includes raylib, so this header is only ever included by app-target TUs
// (never by seads_tests).

#include "raylib.h"

namespace render {

// The shared map face, loaded at `kMapFontBase` px and filtered bilinear so
// DrawTextEx can scale it down cleanly. Implemented in render/tourist_map.cpp.
const Font& map_font();

}  // namespace render
