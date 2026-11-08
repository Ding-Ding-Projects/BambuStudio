#include "PrinterFaultReference.hpp"

#include "I18N.hpp"
#include "PersonalVocabulary.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"

namespace Slic3r { namespace GUI { namespace PrinterFaultReference {

namespace {
// Deliberately meaningless: it identifies the reference line itself, not a fault.
// Keep it stable; personal wording files key on this exact spelling.
constexpr const char *kReferenceCode = "BS-7E3A-91C4";
} // namespace

wxString code()
{
    // remember() grants display trust to this exact fixed string only, the same
    // way translated strings receive it. Dynamic printer text is never trusted.
    return PersonalVocabulary::remember(wxString::FromUTF8(kReferenceCode));
}

::Label *create_label(wxWindow *parent)
{
    auto *label = new ::Label(parent, ::Label::Body_13, code());
    label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    label->SetName(_L("Fault reference"));
    return label;
}

}}} // namespace Slic3r::GUI::PrinterFaultReference
