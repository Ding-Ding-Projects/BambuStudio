// Non-window fixture: the runner inserts actual production method bodies.
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "src/slic3r/GUI/PreferencesSearchTraversal.hpp"
using namespace Slic3r::GUI;

struct wxString : std::string {
    using std::string::string;
    wxString &operator<<(char c) { push_back(c); return *this; }
    wxString &operator<<(const wxString &s) { append(s); return *this; }
};
struct wxStaticText { wxString text; int font = 13; int GetFont() const { return font; } };
struct Label { static constexpr int Head_16 = 16; };
struct wxSizer;
struct wxWindow { wxSizer *sizer = nullptr; std::vector<wxStaticText *> labels; wxSizer *GetSizer() { return sizer; } };
struct wxSizerItem {
    wxSizer *sizer = nullptr; wxWindow *window = nullptr; bool shown = true;
    bool IsSizer() const { return sizer; } bool IsWindow() const { return window; }
    bool IsSpacer() const { return !sizer && !window; } bool IsShown() const { return shown; }
    wxSizer *GetSizer() { return sizer; } wxWindow *GetWindow() { return window; }
    void Show(bool value) { shown = value; }
};
struct wxSizer { std::vector<wxSizerItem *> items; const auto &GetChildren() const { return items; } };
struct Book { std::vector<wxWindow *> pages; size_t GetPageCount() { return pages.size(); } wxWindow *GetPage(size_t i) { return pages.at(i); } };
struct PreferencesDialog {
    struct OptionRow { std::string key; wxSizer *sizer; wxWindow *window; };
    struct SearchRow { int page; wxSizerItem *item; bool baseline_shown; std::vector<wxStaticText *> labels; wxString haystack; bool is_title; std::vector<std::string> keys; };
    Book *m_book; std::vector<OptionRow> m_option_rows; std::vector<SearchRow> m_search_rows;
    void build_search_index();
};
void collect_search_labels_from_window(wxWindow *window, std::vector<wxStaticText *> &labels);
void collect_search_labels_from_sizer(wxSizer *sizer, std::vector<wxStaticText *> &labels) {
    if (!sizer) return;
    for (auto *item : sizer->items) {
        if (item->window) collect_search_labels_from_window(item->window, labels);
        else if (item->sizer) collect_search_labels_from_sizer(item->sizer, labels);
    }
}
void collect_search_labels_from_window(wxWindow *window, std::vector<wxStaticText *> &labels) {
    labels.insert(labels.end(), window->labels.begin(), window->labels.end());
    collect_search_labels_from_sizer(window->sizer, labels);
}
wxString search_label_text(wxStaticText *label) { return label->text; }

namespace MD3 { namespace Metrics {
struct Metric { int radius; };
inline constexpr Metric comfortable{16}, compact{12};
inline bool compact_mode = false;
bool isCompact() { return compact_mode; }
const Metric &active() { return compact_mode ? compact : comfortable; }
}}
struct StaticBox {
    enum class Density { Compact, Comfortable };
    bool m_uses_default_radius = true; double m_default_radius_dip = 12, radius = 12, scale = 1;
    double FromDIP(double value) const { return value * scale; }
    void Refresh() {}
    void SetCornerRadius(double); void SetDefaultCornerRadius(double); void RescaleDefaultCornerRadius(); void SetDensity(Density);
};
double FromDIP(double value) { return value; }
// PRODUCTION_METHODS

void expect(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct Tree {
    wxStaticText title{"Schedules"}, alpha{"Language rule"}, beta{"Theme rule"};
    wxWindow title_window{nullptr,{&title}}, alpha_window{nullptr,{&alpha}}, beta_window{nullptr,{&beta}};
    wxSizerItem title_item{nullptr,&title_window}, alpha_label{nullptr,&alpha_window}, beta_label{nullptr,&beta_window};
    wxSizer alpha_sizer{{&alpha_label}}, beta_sizer{{&beta_label}};
    wxSizerItem alpha_row{&alpha_sizer}, beta_row{&beta_sizer};
    wxSizer contents{{&title_item,&alpha_row,&beta_row}};
    wxWindow card{&contents}; wxSizerItem card_item{nullptr,&card};
    wxSizer page_sizer{{&card_item}}; wxWindow page{&page_sizer}; Book book{{&page}};
    PreferencesDialog dialog{&book,{{"scheduled_settings",&alpha_sizer,nullptr},{"scheduled_settings",&beta_sizer,nullptr}}, {}};
};
int passed = 0;
template<class F> void run(const char *name, F body) { body(); ++passed; std::cout << "PASS " << name << '\n'; }
int main(int argc, char **argv) {
    try {
        const std::string only = argc > 1 ? argv[1] : "all";
        if (only != "radius") {
            run("nested registered rows retain exact keys and individual text", [] {
                Tree t; t.dialog.build_search_index();
                expect(t.dialog.m_search_rows.size()==3,"nested card was indexed instead of its rows");
                const auto &a=t.dialog.m_search_rows[1], &b=t.dialog.m_search_rows[2];
                expect(a.item==&t.alpha_row && b.item==&t.beta_row,"row identity changed");
                expect(a.keys==std::vector<std::string>{"scheduled_settings"},"precise teleport key missing");
                expect(a.haystack=="Language rule" && b.haystack=="Theme rule","row text collapsed into card");
            });
            run("filtering rows cannot hide a matching ancestor card", [] {
                Tree t; t.dialog.build_search_index();
                for (auto &row:t.dialog.m_search_rows) row.item->Show(row.baseline_shown && row.haystack.find("Language")!=std::string::npos);
                expect(t.card_item.shown && t.alpha_row.shown && !t.beta_row.shown,"matching row hidden or unrelated row retained");
                for(auto &row:t.dialog.m_search_rows) row.item->Show(row.baseline_shown);
                expect(t.alpha_row.shown && t.beta_row.shown && t.card_item.shown,"reset changed ancestor visibility");
            });
            run("hidden rows and hidden card ancestry retain baseline visibility", [] {
                Tree t; t.beta_row.shown=false; t.dialog.build_search_index();
                expect(!t.dialog.m_search_rows.back().baseline_shown,"hidden row was revealed");
                t.card_item.shown=false; t.dialog.build_search_index();
                expect(std::none_of(t.dialog.m_search_rows.begin(),t.dialog.m_search_rows.end(),[](const auto &r){return r.baseline_shown;}),"hidden card ancestry lost");
            });
            run("registered window boundary stops before nested controls", [] {
                Tree t; t.dialog.m_option_rows.push_back({"whole_card",nullptr,&t.card}); t.dialog.build_search_index();
                expect(t.dialog.m_search_rows.size()==1 && t.dialog.m_search_rows[0].item==&t.card_item,"registered window flattened");
                expect(t.dialog.m_search_rows[0].keys==std::vector<std::string>{"whole_card"},"window registration identity lost");
            });
            run("unregistered immediate compound rows stay atomic", [] {
                Tree t; t.dialog.m_option_rows.clear(); t.dialog.build_search_index();
                expect(t.dialog.m_search_rows.size()==1 && t.dialog.m_search_rows[0].item==&t.card_item,"unregistered compound row flattened");
            });
            run("registered sizer boundary stops before deeper registrations", [] {
                Tree t; t.dialog.m_option_rows.push_back({"nested_control",nullptr,&t.alpha_window}); t.dialog.build_search_index();
                expect(t.dialog.m_search_rows.size()==3 && t.dialog.m_search_rows[1].keys.size()==1,"registered row boundary flattened");
            });
            run("nested structural wrappers retain immediate rows and ignore spacers", [] {
                Tree t; wxSizer wrapper{{&t.card_item}}; wxSizerItem wrapped{&wrapper}, spacer;
                wxStaticText note{"Unregistered note"}; wxWindow note_window{nullptr,{&note}};
                wxSizerItem note_item{nullptr,&note_window};
                t.page_sizer.items={&note_item,&spacer,&wrapped}; t.dialog.build_search_index();
                expect(t.dialog.m_search_rows.size()==4,"wrapper or spacer became a row");
                expect(t.dialog.m_search_rows.front().item==&note_item,"unregistered immediate row lost");
                expect(t.dialog.m_search_rows[2].item==&t.alpha_row && t.dialog.m_search_rows[2].keys.size()==1,"nested registered identity lost");
            });
        }
        if (only != "rows") {
            run("all three actual card initializers retain DPI and density lifecycle", [] {
                for(int index=0;index<3;++index) for(bool compact:{false,true}) {
                    StaticBox box; MD3::Metrics::compact_mode=compact; initialize_card(box,index);
                    expect(box.m_uses_default_radius,"card initializer pinned radius");
                    for(double scale:{1.,1.25,1.5,2.}) { box.scale=scale; box.RescaleDefaultCornerRadius(); expect(box.radius==(compact?12:16)*scale,"DPI rescale ignored"); }
                    box.SetDensity(compact?StaticBox::Density::Comfortable:StaticBox::Density::Compact);
                    expect(box.radius==(compact?16:12)*2,"density change ignored");
                }
            });
            run("explicit radius override remains distinct from default lifecycle", [] {
                StaticBox box; box.SetCornerRadius(7); box.scale=2; box.SetDensity(StaticBox::Density::Comfortable); box.RescaleDefaultCornerRadius();
                expect(box.radius==7 && !box.m_uses_default_radius,"fixture lost explicit override semantics");
            });
        }
        std::cout << passed << "/" << passed << " cases passed; non-window production-method fixture\n";
        return 0;
    } catch(const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
