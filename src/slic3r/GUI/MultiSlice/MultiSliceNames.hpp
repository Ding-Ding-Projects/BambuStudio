#pragma once

#include <set>
#include <string>

// File names for a multi-slice run, free of wxWidgets so they can be tested on
// their own. One sliced file is written per ink: "<base>_<ink>.gcode.3mf".
namespace Slic3r::GUI::MultiSlice {

// The ink name reduced to characters every file system and printer screen
// accepts: letters, digits, '-', '+' and '.' stay, any other run becomes one
// '_'. Returns "ink" when nothing is left.
std::string file_safe(const std::string &ink_name);

// "<base>_<file_safe(ink)>.gcode.3mf", with " (2)", " (3)" ... added before the
// extension when the name is already in `taken`. The returned name is added to
// `taken`.
std::string variant_file_name(const std::string &base, const std::string &ink_name, std::set<std::string> &taken);

} // namespace Slic3r::GUI::MultiSlice
