#pragma once

// Rendering of the selected app logo for the app chrome: the title bar tile,
// the window and taskbar icon, the About banner and the startup screen. It
// draws only from AppLogo::Settings, never reads a file, and never touches
// application or package identity or the installed operating-system icon.

#include "libslic3r/AppLogo/Logo.hpp"
#include <wx/bitmap.h>
#include <wx/iconbndl.h>

namespace Slic3r { namespace GUI { namespace AppLogoUI {

// The selected logo as a square bitmap of exactly `pixels` device pixels. It
// is generated at no more than AppLogo::max_output_px and resampled up past
// that bound. Invalid for the shipped mark: callers then draw their own
// shipped asset.
wxBitmap bitmap(const AppLogo::Settings& settings, unsigned pixels);
// For a surface of `logical` pixels at display `scale`: bitmap() at
// AppLogo::plan().draw_px with the bitmap's scale factor set to match.
wxBitmap scaled_bitmap(const AppLogo::Settings& settings, unsigned logical, double scale);
// One icon per AppLogo::window_icon_sizes entry, each rendered from the
// source. Empty for the shipped mark.
wxIconBundle icon_bundle(const AppLogo::Settings& settings);
// `base` with the selected logo drawn over the `side` x `side` square at
// (left, top), on a white rounded tile when the logo is transparent. Done in
// pixel arithmetic rather than through a device context, so an alpha bitmap
// keeps correct alpha on every platform. Returns `base` unchanged for the
// shipped mark or when the logo cannot be rendered.
wxBitmap overlay(const wxBitmap& base, const AppLogo::Settings& settings, int left, int top, unsigned side);

} } }
