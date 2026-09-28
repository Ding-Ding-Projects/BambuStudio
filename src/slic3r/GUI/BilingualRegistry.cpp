#include "BilingualRegistry.hpp"

#include <algorithm>
#include <cwctype>
#include <mutex>

namespace Slic3r { namespace GUI { namespace I18N {

namespace {

bool is_space(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0x3000; }

// One placeholder token at text[pos], or 0 when there is none. `slot` receives
// the position number of %2$s / %2% (1-based), or 0 for sequential tokens.
size_t placeholder_length(const std::wstring &text, size_t pos, int &slot)
{
    slot = 0;
    if (pos >= text.size())
        return 0;
    const wchar_t c = text[pos];
    if (c == L'{') {
        // {}, {name}, {{name}}
        const bool doubled = pos + 1 < text.size() && text[pos + 1] == L'{';
        const size_t close = text.find(doubled ? L"}}" : L"}", pos);
        if (close == std::wstring::npos || close - pos > 40)
            return 0;
        return close - pos + (doubled ? 2 : 1);
    }
    if (c != L'%' || pos + 1 >= text.size())
        return 0;
    size_t i = pos + 1;
    if (text[i] == L'%')
        return 0; // literal percent, handled by the caller
    size_t digits = i;
    while (digits < text.size() && std::iswdigit(text[digits]))
        ++digits;
    if (digits > i && digits < text.size() && text[digits] == L'%') {
        slot = std::stoi(text.substr(i, digits - i)); // boost %1%
        return digits - pos + 1;
    }
    if (digits > i && digits < text.size() && text[digits] == L'$') {
        slot = std::stoi(text.substr(i, digits - i)); // printf %1$s
        i = digits + 1;
    }
    while (i < text.size() && std::wcschr(L"-+ #0'", text[i]) != nullptr)
        ++i;
    while (i < text.size() && (std::iswdigit(text[i]) || text[i] == L'*'))
        ++i;
    if (i < text.size() && text[i] == L'.') {
        ++i;
        while (i < text.size() && (std::iswdigit(text[i]) || text[i] == L'*'))
            ++i;
    }
    while (i < text.size() && std::wcschr(L"hljztLI", text[i]) != nullptr)
        ++i;
    if (i < text.size() && std::wcschr(L"diuoxXfFeEgGaAcspn", text[i]) != nullptr)
        return i - pos + 1;
    slot = 0;
    return 0;
}

// Splits a template into literal pieces and placeholder slots. Returns false
// when the text has no placeholder.
bool split_template(const std::wstring &text, std::vector<std::wstring> &literals, std::vector<int> &slots,
                    std::vector<std::wstring> *tokens = nullptr)
{
    literals.assign(1, std::wstring());
    slots.clear();
    for (size_t i = 0; i < text.size();) {
        if (text[i] == L'%' && i + 1 < text.size() && text[i + 1] == L'%') {
            literals.back() += L'%';
            i += 2;
            continue;
        }
        int slot = 0;
        const size_t length = placeholder_length(text, i, slot);
        if (length == 0) {
            literals.back() += text[i++];
            continue;
        }
        slots.push_back(slot);
        if (tokens)
            tokens->push_back(text.substr(i, length));
        literals.emplace_back();
        i += length;
    }
    return !slots.empty();
}

size_t letter_count(const std::wstring &text)
{
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](wchar_t c) { return std::iswalpha(c) != 0; }));
}

// Trailing decorations a caller may add to a translated string before showing it.
struct Suffix {
    const wchar_t *english;
    const wchar_t *cantonese;
};
const Suffix SUFFIXES[] = {
    { L"...", L"…" }, { L"…", L"…" }, { L":", L"：" }, { L"：", L"：" }, { L" *", L" *" }, { L"*", L"*" },
};

} // namespace

BilingualRegistry &BilingualRegistry::instance()
{
    static BilingualRegistry registry;
    return registry;
}

void BilingualRegistry::reset(bool enabled)
{
    {
        std::unique_lock lock(m_mutex);
        m_exact.clear();
        m_template_index.clear();
        m_templates.clear();
        m_enabled.store(enabled, std::memory_order_relaxed);
    }
    std::lock_guard cache_lock(m_cache_mutex);
    m_cache.clear();
}

size_t BilingualRegistry::size() const
{
    std::shared_lock lock(m_mutex);
    return m_exact.size() + m_templates.size();
}

void BilingualRegistry::set_managed(const void *window, bool managed)
{
    if (window == nullptr)
        return;
    std::lock_guard lock(m_managed_mutex);
    if (managed)
        m_managed.insert(window);
    else
        m_managed.erase(window);
}

bool BilingualRegistry::is_managed(const void *window) const
{
    std::lock_guard lock(m_managed_mutex);
    return m_managed.count(window) != 0;
}

std::wstring BilingualRegistry::normalize(const wxString &text)
{
    std::wstring in = text.ToStdWstring();
    const size_t tab = in.find(L'\t');
    if (tab != std::wstring::npos)
        in.erase(tab);
    std::wstring out;
    out.reserve(in.size());
    bool pending_space = false;
    for (size_t i = 0; i < in.size(); ++i) {
        wchar_t c = in[i];
        if (c == L'&') {
            if (i + 1 < in.size() && in[i + 1] == L'&') {
                c = L'&';
                ++i;
            } else {
                continue; // mnemonic marker
            }
        }
        if (is_space(c)) {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space) {
            out += L' ';
            pending_space = false;
        }
        out += c;
    }
    return out;
}

wxString BilingualRegistry::strip_mnemonic(const wxString &text)
{
    std::wstring value = text.ToStdWstring();
    // "檔案(&F)" and "檔案（&F）" lose the whole bracket; any other '&' marker is dropped.
    for (const wchar_t *open : { L"(&", L"（&" }) {
        size_t pos = 0;
        while ((pos = value.find(open, pos)) != std::wstring::npos) {
            const size_t close = value.find_first_of(L")）", pos);
            if (close != std::wstring::npos && close - pos <= 4)
                value.erase(pos, close - pos + 1);
            else
                pos += 2;
        }
    }
    std::wstring out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'&') {
            if (i + 1 < value.size() && value[i + 1] == L'&') {
                out += L'&';
                ++i;
            }
            continue;
        }
        out += value[i];
    }
    return wxString(out);
}

void BilingualRegistry::record(const wxString &english, const wxString &cantonese)
{
    if (!enabled() || english.empty() || cantonese.empty())
        return;
    const std::wstring key = normalize(english);
    if (key.empty())
        return;
    {
        std::shared_lock lock(m_mutex);
        if (m_exact.count(key) != 0 || m_template_index.count(key) != 0)
            return;
    }
    const std::wstring value = strip_mnemonic(cantonese).ToStdWstring();
    if (value.empty() || value == key)
        return;
    std::vector<std::wstring> literals;
    std::vector<int>          slots;
    bool added = false;
    if (split_template(key, literals, slots)) {
        added = add_template(key, value);
    } else {
        std::unique_lock lock(m_mutex);
        added = m_exact.emplace(key, value).second;
    }
    if (added) {
        // A cached miss may now have an answer.
        std::lock_guard cache_lock(m_cache_mutex);
        m_cache.clear();
    }
}

bool BilingualRegistry::add_template(const std::wstring &english, const std::wstring &cantonese)
{
    Template tpl;
    tpl.english   = english;
    tpl.cantonese = cantonese;
    split_template(english, tpl.literals, tpl.english_slots);

    // Ambiguity guards: two placeholders with nothing between them cannot be
    // told apart, and a template with almost no literal text matches anything.
    for (size_t i = 1; i + 1 < tpl.literals.size(); ++i)
        if (normalize(tpl.literals[i]).empty())
            return false;
    size_t letters = 0;
    for (const std::wstring &literal : tpl.literals) {
        letters += letter_count(literal);
        if (literal.size() > tpl.anchor.size())
            tpl.anchor = literal;
    }
    if (letters < 4 || tpl.anchor.size() < 3)
        return false;

    // The Cantonese must use the same number of placeholders.
    std::vector<std::wstring> cantonese_literals;
    std::vector<int>          cantonese_slots;
    split_template(cantonese, cantonese_literals, cantonese_slots);
    if (cantonese_slots.size() != tpl.english_slots.size())
        return false;

    std::unique_lock lock(m_mutex);
    if (m_template_index.count(english) != 0)
        return true;
    m_template_index.emplace(english, m_templates.size());
    m_templates.push_back(std::move(tpl));
    return true;
}

std::wstring BilingualRegistry::fill_template(const Template &tpl, const std::vector<std::wstring> &captures)
{
    std::vector<std::wstring> literals;
    std::vector<int>          slots;
    std::vector<std::wstring> tokens;
    split_template(tpl.cantonese, literals, slots, &tokens);
    std::wstring out = literals.front();
    size_t sequential = 0;
    for (size_t i = 0; i < slots.size(); ++i) {
        size_t capture = captures.size();
        if (slots[i] > 0) {
            // A numbered placeholder takes the English capture with the same number.
            for (size_t e = 0; e < tpl.english_slots.size(); ++e)
                if (tpl.english_slots[e] == slots[i]) { capture = e; break; }
        } else {
            // The k-th unnumbered placeholder takes the k-th unnumbered English capture.
            size_t seen = 0;
            for (size_t e = 0; e < tpl.english_slots.size(); ++e) {
                if (tpl.english_slots[e] != 0)
                    continue;
                if (seen++ == sequential) { capture = e; break; }
            }
            ++sequential;
        }
        if (capture >= captures.size())
            return std::wstring();
        out += captures[capture];
        out += literals[i + 1];
    }
    return out;
}

std::wstring BilingualRegistry::lookup_exact(const std::wstring &key) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_exact.find(key);
    return it == m_exact.end() ? std::wstring() : it->second;
}

std::wstring BilingualRegistry::lookup_template(const std::wstring &key) const
{
    std::shared_lock lock(m_mutex);
    for (const Template &tpl : m_templates) {
        if (key.find(tpl.anchor) == std::wstring::npos)
            continue;
        // Match literal pieces in order; each placeholder captures the shortest
        // run up to the next literal (the last one runs to the end).
        std::vector<std::wstring> captures;
        size_t pos = 0;
        bool ok = key.compare(0, tpl.literals.front().size(), tpl.literals.front()) == 0;
        if (!ok)
            continue;
        pos = tpl.literals.front().size();
        for (size_t i = 1; ok && i < tpl.literals.size(); ++i) {
            const std::wstring &literal = tpl.literals[i];
            size_t next;
            if (i + 1 == tpl.literals.size()) {
                if (literal.size() > key.size() - pos || key.compare(key.size() - literal.size(), literal.size(), literal) != 0) {
                    ok = false;
                    break;
                }
                next = key.size() - literal.size();
            } else {
                next = key.find(literal, pos);
            }
            if (next == std::wstring::npos || next <= pos) {
                ok = false; // every placeholder must capture something
                break;
            }
            captures.push_back(key.substr(pos, next - pos));
            pos = next + literal.size();
        }
        if (!ok || pos != key.size())
            continue;
        const std::wstring filled = fill_template(tpl, captures);
        if (!filled.empty())
            return filled;
    }
    return std::wstring();
}

wxString BilingualRegistry::lookup(const wxString &label) const
{
    if (!enabled() || label.empty())
        return wxString();
    const std::wstring key = normalize(label);
    if (key.empty())
        return wxString();
    {
        std::lock_guard cache_lock(m_cache_mutex);
        const auto cached = m_cache.find(key);
        if (cached != m_cache.end())
            return wxString(cached->second);
    }
    const std::wstring found = resolve(key);
    std::lock_guard cache_lock(m_cache_mutex);
    if (m_cache.size() > 20000)
        m_cache.clear(); // bound the memory a long session can spend on misses
    m_cache.emplace(key, found);
    return wxString(found);
}

std::wstring BilingualRegistry::resolve(const std::wstring &key) const
{
    std::wstring found = lookup_exact(key);
    if (found.empty()) {
        // "Layer height:" and "Export..." are translated strings with a
        // decoration the caller appended; keep the decoration in Cantonese form.
        for (const Suffix &suffix : SUFFIXES) {
            const std::wstring tail(suffix.english);
            if (key.size() > tail.size() && key.compare(key.size() - tail.size(), tail.size(), tail) == 0) {
                const std::wstring stem = normalize(wxString(key.substr(0, key.size() - tail.size())));
                std::wstring base = lookup_exact(stem);
                if (!base.empty()) {
                    const std::wstring ctail(suffix.cantonese);
                    if (base.size() < ctail.size() || base.compare(base.size() - ctail.size(), ctail.size(), ctail) != 0)
                        base += ctail;
                    found = base;
                }
                break;
            }
        }
    }
    if (found.empty())
        found = lookup_template(key);
    if (found == key)
        found.clear();
    return found;
}

}}} // namespace Slic3r::GUI::I18N
