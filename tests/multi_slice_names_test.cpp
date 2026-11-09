// File names written by Multi-slice: one "<project>_<ink>.gcode.3mf" per ink.
// Built by tests/multi_slice_names.test.mjs as a single-file fixture.
#include "src/slic3r/GUI/MultiSlice/MultiSliceNames.cpp"

#include <cstdio>
#include <set>
#include <string>

namespace {
int failures = 0;

void expect(const std::string &actual, const std::string &expected, const char *what)
{
    if (actual == expected) return;
    ++failures;
    std::printf("FAIL %s: got [%s], expected [%s]\n", what, actual.c_str(), expected.c_str());
}
} // namespace

int main()
{
    using namespace Slic3r::GUI::MultiSlice;

    expect(file_safe("Bambu PLA Basic"), "Bambu_PLA_Basic", "spaces become one separator");
    expect(file_safe("Generic ABS @BBL X1C"), "Generic_ABS_BBL_X1C", "a run of unsafe characters becomes one separator");
    expect(file_safe("PETG-CF+ 0.4"), "PETG-CF+_0.4", "dashes, plus signs and dots stay");
    expect(file_safe("  leading and trailing  "), "leading_and_trailing", "no separator at either end");
    expect(file_safe("Ink..."), "Ink", "Windows drops a trailing dot, so none is written");
    expect(file_safe("a<b>c:d\"e/f\\g|h?i*j"), "a_b_c_d_e_f_g_h_i_j", "reserved file name characters are replaced");
    expect(file_safe("\xe5\xa2\xa8\xe6\xb0\xb4 PLA"), "\xe5\xa2\xa8\xe6\xb0\xb4_PLA", "non-ASCII ink names stay readable");
    expect(file_safe("***"), "ink", "nothing usable left gives a fallback");
    expect(file_safe(""), "ink", "an empty name gives a fallback");

    std::set<std::string> taken;
    expect(variant_file_name("bracket", "Generic PLA", taken), "bracket_Generic_PLA.gcode.3mf", "first file");
    expect(variant_file_name("bracket", "Generic ABS", taken), "bracket_Generic_ABS.gcode.3mf", "second ink");
    expect(variant_file_name("bracket", "Generic PLA", taken), "bracket_Generic_PLA (2).gcode.3mf", "a repeated name is numbered");
    expect(variant_file_name("bracket", "Generic/PLA", taken), "bracket_Generic_PLA (3).gcode.3mf", "names equal after cleaning are numbered too");
    if (taken.size() != 4) {
        ++failures;
        std::printf("FAIL every name is recorded: %zu recorded\n", taken.size());
    }

    if (failures == 0) std::printf("multi-slice names: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
