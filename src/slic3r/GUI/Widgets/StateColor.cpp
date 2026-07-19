#include "StateColor.hpp"
#include <wx/gdicmn.h>

static bool gDarkMode = false;

static bool operator<(wxColour const &l, wxColour const &r) { return l.GetRGBA() < r.GetRGBA(); }

static std::map<wxColour, wxColour> gDarkColors{
    {ThemeColor::BrandGreen,  "#8bd89b"},/*green*/
    {ThemeColor::BrandGreenPressed, "#7ac98a"},
    {ThemeColor::BrandGreenHovered, "#9ee0ad"},
    // {"#1F8EEA", "#2778D2"},/*blue*/ -- dead, only used by disabled Notebook.cpp:80 OnPaint
    {ThemeColor::Warning,     "#ffb77c"},
    {ThemeColor::Danger,      "#ffb4ab"},/*red*/
    {ThemeColor::Link,        "#479EF5"},/*blue*/
    {ThemeColor::TextPrimary, "#e8e7ee"},/*black*/
    {ThemeColor::TextSecondary, "#cdced8"},
    {ThemeColor::TextMuted,     "#a8a9b3"},
    {ThemeColor::TextDisabled,  "#6a6b73"},
    {ThemeColor::White,       "#202127"},
    {ThemeColor::Grey200,     "#202127"},
    {ThemeColor::Grey250,     "#25262b"},
    {ThemeColor::Grey300,     "#2f3036"},/*gray -> */
    {ThemeColor::Grey350,     "#393a41"},
    {ThemeColor::Grey400,     "#4a4c54"},
    {ThemeColor::Grey450,     "#94959f"},
    {"#2C2C2E", "#e8e7ee"},/*black*/
    {"#E5E7EB", "#393a41"},/*gray200 -> gray800*/
    {"#6B6B6B", "#a8a9b3"},/*gray -> */
    {"#ACACAC", "#94959f"},/*gray -> */
    {"#3B4446", "#2f3036"},
    {"#CECECE", "#4a4c54"},
    {"#DBFDD5", "#095228"},
    {"#000000", "#e8e7ee"},
    {"#F4F4F4", "#202127"},
    {"#F7F7F7", "#202127"},
    {"#DBDBDB", "#4a4c54"},
    {"#EDFAF2", "#095228"},
    {"#323A3C", "#e8e7ee"},
    {"#6B6B6A", "#a8a9b3"},
    {"#303A3C", "#e8e7ee"},
    {"#FEFFFF", "#1b1c21"},
    {"#363636", "#e8e7ee"},
    {"#F0F0F1", "#25262b"},
    {"#9E9E9E", "#94959f"},
    {"#D7E8DE", "#2b3a2f"},
    {"#2B3436", "#cdced8"},
    {"#ABABAB", "#94959f"},
    {"#D9D9D9", "#393a41"},
    {"#EBF9F0", "#095228"},
    {"#DBFDE7", "#095228"}
    //{"#F0F0F0", "#4C4C54"},
};

void StateColor::SetDarkMode(bool dark) { gDarkMode = dark; }

inline wxColour darkModeColorFor2(wxColour const &color)
{
    if (!gDarkMode)
        return color;
    auto iter = gDarkColors.find(color);
    if (iter != gDarkColors.end()) return iter->second;
    return color;
}

std::map<wxColour, wxColour> revert(std::map<wxColour, wxColour> const & map)
{
    std::map<wxColour, wxColour> map2;
    for (auto &p : map) map2.emplace(p.second, p.first);
    return map2;
}

wxColour StateColor::lightModeColorFor(wxColour const &color)
{
    static std::map<wxColour, wxColour> gLightColors = revert(gDarkColors);
    auto iter = gLightColors.find(color);
    if (iter != gLightColors.end()) return iter->second;
    return color;
}

wxColour StateColor::darkModeColorFor(wxColour const &color) { return darkModeColorFor2(color); }

StateColor::StateColor(wxColour const &color) { append(color, 0); }

StateColor::StateColor(wxString const &color) { append(color, 0); }

StateColor::StateColor(unsigned long color) { append(color, 0); }

void StateColor::append(wxColour const & color, int states)
{
    statesList_.push_back(states);
    colors_.push_back(color);
}

void StateColor::append(wxString const & color, int states)
{
    wxColour c1(color);
    append(c1, states);
}

void StateColor::append(unsigned long color, int states)
{
    if ((color & 0xff000000) == 0)
        color |= 0xff000000;
    wxColour cl; cl.SetRGBA(color & 0xff00ff00 | ((color & 0xff) << 16) | ((color >> 16) & 0xff));
    append(cl, states);
}

void StateColor::clear()
{
    statesList_.clear();
    colors_.clear();
}

int StateColor::states() const
{
    int states = 0;
    for (auto s : statesList_) states |= s;
    states = (states & 0xffff) | (states >> 16);
    if (takeFocusedAsHovered_ && (states & Hovered))
        states |= Focused;
    return states;
}

wxColour StateColor::defaultColor() {
    return colorForStates(0);
}

wxColour StateColor::colorForStates(int states)
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return darkModeColorFor2(colors_[i]);
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return darkModeColorFor2(colors_[i]);
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

wxColour StateColor::colorForStatesNoDark(int states)
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return colors_[i];
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return colors_[i];
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

int StateColor::colorIndexForStates(int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        int s   = statesList_[i];
        int on  = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) { return i; }
    }
    return -1;
}

bool StateColor::setColorForStates(wxColour const &color, int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        if (statesList_[i] == states) {
            colors_[i] = color;
            return true;
        }
    }
    return false;
}

void StateColor::setTakeFocusedAsHovered(bool set) { takeFocusedAsHovered_ = set; }

StateColor StateColor::createButtonStyleGray()
{
    return StateColor(std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed),
        std::pair<wxColour, int>(*wxWHITE, StateColor::Focused),
        std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
        std::pair<wxColour, int>(*wxWHITE, StateColor::Normal));
}
