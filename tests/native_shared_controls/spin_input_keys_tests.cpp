#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

// Non-window adapters for the kit SpinInput (src/slic3r/GUI/Widgets/SpinInput.cpp).
// spin_input_keys.test.mjs extracts the production bodies into spin_input_keys.inc:
//   SPIN_KEYS:  SpinInput::SetValue(int), SpinInput::keyPressed and SpinInput::onTextEnter
//   SPIN_CHARS: spinAcceptsChar, the rule for a typed character
// The stubs below stand in for wxWidgets; the logic under test is the extracted code.
enum { WXK_SPACE = 32, WXK_DELETE = 127, WXK_LEFT = 314, WXK_UP = 315, WXK_RIGHT = 316, WXK_DOWN = 317 };

// wxString::ToLong accepts the whole text as one number, as strtol reads it.
struct wxString : std::string {
    wxString() = default;
    wxString(const char *text) : std::string(text) {}
    wxString(const std::string &text) : std::string(text) {}
    static wxString FromDouble(double value) { return std::to_string(long(value)); }
    bool ToLong(long *out) const
    {
        if (empty()) return false;
        char *end = nullptr;
        errno = 0;
        const long value = std::strtol(c_str(), &end, 10);
        if (errno != 0 || end == c_str() || *end != '\0') return false;
        *out = value;
        return true;
    }
};

#if defined(SPIN_KEYS)
struct wxKeyEvent {
    int code;
    bool skipped = false;
    int GetKeyCode() const { return code; }
    void Skip(bool skip = true) { skipped = skip; }
};
struct wxCommandEvent {
    int id = 0;
    void SetId(int value) { id = value; }
};
struct TextCtrlStub {
    wxString value;
    wxString GetValue() const { return value; }
    void SetValue(const wxString &text) { value = text; }
};
struct SpinInput {
    TextCtrlStub editor;
    TextCtrlStub *text_ctrl = &editor;
    int val = 0, min = 0, max = 100;
    bool text_updating = false;
    int spin_events = 0;
    int GetId() const { return 7; }
    void ProcessEventLocally(wxCommandEvent &) {}
    void sendSpinEvent() { ++spin_events; }
    void SetValue(int value);
    void keyPressed(wxKeyEvent &event);
    void onTextEnter(wxCommandEvent &event);
};
#include "spin_input_keys.inc"

int main()
{
    int cases = 0, assertions = 0, failures = 0;
    auto check = [&](bool ok, const char *what) {
        ++assertions;
        if (!ok) { ++failures; std::cout << "FAIL: " << what << '\n'; }
    };
    // One spin field in [min, max] showing `shown` while holding `value`.
    auto field = [](int min, int max, int value, const char *shown = nullptr) {
        SpinInput spin;
        spin.min = min;
        spin.max = max;
        spin.SetValue(value);
        if (shown) spin.editor.value = shown;
        return spin;
    };
    auto press = [](SpinInput &spin, int key) { wxKeyEvent event{key}; spin.keyPressed(event); return event.skipped; };

    { ++cases; // Up reaches the maximum, as wxSpinCtrl did.
        auto spin = field(0, 10, 9);
        press(spin, WXK_UP);
        check(spin.val == 10 && spin.editor.value == "10", "Up from max - 1 reaches max");
        check(spin.spin_events == 1, "reaching max sends one spin event");
        press(spin, WXK_UP);
        check(spin.val == 10 && spin.spin_events == 1, "Up at max stays and sends nothing");
    }
    { ++cases; // Down reaches the minimum.
        auto spin = field(0, 10, 1);
        press(spin, WXK_DOWN);
        check(spin.val == 0 && spin.editor.value == "0" && spin.spin_events == 1, "Down from min + 1 reaches min");
        press(spin, WXK_DOWN);
        check(spin.val == 0 && spin.spin_events == 1, "Down at min stays and sends nothing");
    }
    { ++cases; // A range below zero (narrator rate and pitch are -10..10).
        auto spin = field(-10, 10, 9);
        press(spin, WXK_UP);
        check(spin.val == 10, "Up reaches max in -10..10");
        spin.SetValue(-9);
        press(spin, WXK_DOWN);
        check(spin.val == -10 && spin.editor.value == "-10", "Down reaches min in -10..10");
    }
    { ++cases; // A one-value range never moves.
        auto spin = field(5, 5, 5);
        press(spin, WXK_UP);
        press(spin, WXK_DOWN);
        check(spin.val == 5 && spin.spin_events == 0, "a one-value range never moves");
    }
    { ++cases; // A typed value outside the range is pulled back before the step.
        auto spin = field(0, 10, 10, "50");
        press(spin, WXK_UP);
        check(spin.val == 10 && spin.editor.value == "10", "Up over a typed 50 shows the maximum");
        check(spin.spin_events == 0, "the held value did not change, so no spin event");
        spin.editor.value = "50";
        press(spin, WXK_DOWN);
        check(spin.val == 9 && spin.editor.value == "9" && spin.spin_events == 1, "Down over a typed 50 steps down from the maximum");
        auto low = field(-10, 10, -10, "-40");
        press(low, WXK_DOWN);
        check(low.val == -10 && low.editor.value == "-10" && low.spin_events == 0, "Down under a typed -40 shows the minimum");
    }
    { ++cases; // Text that is not a number steps from the held value.
        auto spin = field(0, 10, 4, "-");
        press(spin, WXK_UP);
        check(spin.val == 5 && spin.editor.value == "5", "a lone minus steps from the held value");
    }
    { ++cases; // Other keys go on to the editor.
        auto spin = field(0, 10, 4);
        check(press(spin, WXK_LEFT) && spin.val == 4, "Left is skipped to the editor");
    }
    { ++cases; // Enter keeps a typed negative value inside the range.
        auto spin = field(-10, 10, 0, "-50");
        wxCommandEvent enter;
        spin.onTextEnter(enter);
        check(spin.val == -10 && spin.editor.value == "-10" && spin.spin_events == 1, "Enter clamps -50 to -10");
        spin.editor.value = "-7";
        spin.onTextEnter(enter);
        check(spin.val == -7 && spin.spin_events == 2, "Enter keeps -7");
    }
    std::cout << cases << " key cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures ? 1 : 0;
}

#elif defined(SPIN_CHARS)
#include "spin_input_keys.inc"

int main()
{
    int cases = 0, assertions = 0, failures = 0;
    auto check = [&](bool ok, const char *what) {
        ++assertions;
        if (!ok) { ++failures; std::cout << "FAIL: " << what << '\n'; }
    };
    // Typing `ch` over the selection [from, to) of `text` in a field whose minimum is `min`.
    auto typed = [](const char *text, long from, long to, int ch, int min) { return spinAcceptsChar(wxString(text), from, to, ch, min); };

    { ++cases; // Digits are typed anywhere after the sign.
        check(typed("", 0, 0, '7', 0), "a digit in an empty field");
        check(typed("12", 1, 1, '0', 0), "a digit inside a number");
        check(typed("-5", 1, 1, '3', -10), "a digit right after the sign");
        check(typed("-5", 2, 2, '3', -10), "a digit at the end of a negative number");
        check(typed("-5", 0, 1, '3', -10), "a digit replacing the selected sign");
    }
    { ++cases; // A minus sign only when the range reaches below zero.
        check(typed("", 0, 0, '-', -10), "a leading minus when the minimum is -10");
        check(typed("5", 0, 0, '-', -10), "a minus typed ahead of a number");
        check(typed("-5", 0, 1, '-', -10), "a minus replacing the selected sign");
        check(typed("12", 0, 2, '-', -1), "a minus replacing the whole selection");
        check(!typed("", 0, 0, '-', 0), "no minus when the minimum is 0");
        check(!typed("5", 0, 0, '-', 30), "no minus when the minimum is 30");
    }
    { ++cases; // The sign stays the first character, and single.
        check(!typed("5", 1, 1, '-', -10), "no minus after a digit");
        check(!typed("-5", 0, 0, '-', -10), "no second minus ahead of the sign");
        check(!typed("-5", 1, 1, '-', -10), "no second minus after the sign");
        check(!typed("-5", 0, 0, '4', -10), "no digit ahead of the sign");
    }
    { ++cases; // Nothing else that is printable.
        for (int ch : {'+', '.', ',', 'e', 'E', 'x', ' '})
            check(!typed("5", 1, 1, ch, -10), "no sign, separator, exponent or letter");
        check(!typed("5", 1, 1, 0xFF11, -10), "no full-width digit");
    }
    { ++cases; // Control keys, navigation (no character) and Delete pass, as through wxTextValidator.
        for (int ch : {0, 1, 3, 8, 9, 13, 22, 24, 26, int(WXK_DELETE)})
            check(typed("-5", 0, 0, ch, 0), "a control key, navigation or Delete is never filtered");
    }
    std::cout << cases << " character cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures ? 1 : 0;
}
#endif
