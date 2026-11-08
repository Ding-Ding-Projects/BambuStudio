#ifndef slic3r_GUI_Extensions_BeforeSendCheck_hpp_
#define slic3r_GUI_Extensions_BeforeSendCheck_hpp_

// The print.before_send hook in the print dialog: when Send is pressed, every
// extension that is on for it answers first, one at a time, in a window that
// shows what each one is doing and then what it answered. Nothing to look at
// sends at once; rows that warn or fail, notes and runs without a valid answer
// wait for the person; a valid answer that blocks stops the print. The rules
// live in the wx-free summarise_send_check (ExtensionCore).

#include "ExtensionContext.hpp"

#include <string>

class wxWindow;

namespace Slic3r { namespace GUI { namespace Extensions {

enum class SendDecision {
    Send,           // carry on sending
    Back,           // stay in the print dialog
    OpenExtensions, // close the print dialog and open File > Extensions at `extension`
};

struct SendCheckOutcome
{
    SendDecision decision = SendDecision::Send;
    std::string  extension; // OpenExtensions: the extension to select
};

// Returns Send at once, without a window, when no extension is on for print.before_send.
SendCheckOutcome check_before_send(wxWindow *parent, const SendFacts &send);

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_BeforeSendCheck_hpp_
