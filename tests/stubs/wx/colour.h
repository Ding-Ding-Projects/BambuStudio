// A stand-in for <wx/colour.h> with just what src/slic3r/GUI/Widgets/MD3Tokens.hpp
// uses, so its colour rules compile in a test without wxWidgets
// (tests/md3_quiet_colours_test.cpp).
#pragma once
#include <cstdlib>
#include <cstring>

class wxColour
{
public:
    wxColour() = default;
    wxColour(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) : m_r(r), m_g(g), m_b(b), m_a(a), m_ok(true) {}
    wxColour(const char *hex)
    {
        if (hex == nullptr || hex[0] != '#' || std::strlen(hex) != 7) return;
        const long v = std::strtol(hex + 1, nullptr, 16);
        m_r          = static_cast<unsigned char>((v >> 16) & 0xFF);
        m_g          = static_cast<unsigned char>((v >> 8) & 0xFF);
        m_b          = static_cast<unsigned char>(v & 0xFF);
        m_ok         = true;
    }
    unsigned char Red() const { return m_r; }
    unsigned char Green() const { return m_g; }
    unsigned char Blue() const { return m_b; }
    unsigned char Alpha() const { return m_a; }
    bool          IsOk() const { return m_ok; }
    bool          operator==(const wxColour &o) const { return m_r == o.m_r && m_g == o.m_g && m_b == o.m_b && m_a == o.m_a; }
    bool          operator!=(const wxColour &o) const { return !(*this == o); }

private:
    unsigned char m_r = 0, m_g = 0, m_b = 0, m_a = 255;
    bool          m_ok = false;
};
