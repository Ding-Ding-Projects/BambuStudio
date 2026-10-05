#ifndef slic3r_GUI_HumanDate_hpp_
#define slic3r_GUI_HumanDate_hpp_

#include <wx/datetime.h>
#include "I18N.hpp"

namespace Slic3r { namespace GUI { namespace HumanDate {

// Input is already a local civil date. Never perform a zone conversion here.
inline wxString english(const wxDateTime &date)
{
    return date.IsValid() ? wxString::Format("%d %s %d", date.GetDay(),
        wxDateTime::GetEnglishMonthName(date.GetMonth()), date.GetYear()) : wxString();
}

inline wxString cantonese(const wxDateTime &date)
{
    return date.IsValid() ? wxString::Format(wxString::FromUTF8("%d年%d月%d日"),
        date.GetYear(), int(date.GetMonth()) + 1, date.GetDay()) : wxString();
}

inline wxString present(const wxString &en, const wxString &yue)
{
    const auto mode = I18N::language_mode_profile().kind;
    if (mode == I18N::LanguageModeKind::CantoneseHongKong) return yue;
    if (mode == I18N::LanguageModeKind::BilingualEnglishCantoneseHongKong)
        return en + " / " + yue;
    return en;
}

inline wxString format(const wxDateTime &date)
{
    return date.IsValid() ? present(english(date), cantonese(date)) : wxString();
}

inline wxString month_name(wxDateTime::Month month)
{
    return present(wxDateTime::GetEnglishMonthName(month),
        wxString::Format(wxString::FromUTF8("%d月"), int(month) + 1));
}

inline wxString month_year(const wxDateTime &date)
{
    if (!date.IsValid()) return wxString();
    return present(wxString::Format("%s %d", wxDateTime::GetEnglishMonthName(date.GetMonth()), date.GetYear()),
        wxString::Format(wxString::FromUTF8("%d年%d月"), date.GetYear(), int(date.GetMonth()) + 1));
}

inline wxString year(const wxDateTime &date)
{
    if (!date.IsValid()) return wxString();
    return present(wxString::Format("%d", date.GetYear()),
        wxString::Format(wxString::FromUTF8("%d年"), date.GetYear()));
}

inline wxString date_time(const wxDateTime &date, bool seconds = false)
{
    return date.IsValid() ? format(date) + " " + date.Format(seconds ? "%H:%M:%S" : "%H:%M") : wxString();
}

// UTC wire timestamps are converted once, at this presentation boundary.
inline wxDateTime utc_stamp(const wxString &stamp)
{
    wxDateTime date;
    if (stamp.length() != 20 || stamp.Last() != 'Z' ||
        !date.ParseISOCombined(stamp.Left(19), 'T')) return wxDateTime();
    if (date.FormatISOCombined('T') != stamp.Left(19)) return wxDateTime();
    date.MakeFromUTC();
    return date;
}

// Calendar values have no zone and must retain their original day.
inline wxDateTime calendar(const wxString &value)
{
    wxDateTime date;
    if (value.length() != 10 || !date.ParseISODate(value) || date.FormatISODate() != value)
        return wxDateTime();
    return date;
}

}}}
#endif
