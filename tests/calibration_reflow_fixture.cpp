#include "src/slic3r/GUI/CalibrationLayout.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace Slic3r { namespace GUI {
struct wxSize { int x,y; wxSize(int x_=0,int y_=0):x(x_),y(y_){} };
std::vector<std::function<void()>> pending;
struct wxWindow {
    virtual ~wxWindow() = default;
    wxWindow *parent=nullptr;
    wxSize client{100,100};
    bool shown=true;
    int layouts=0;
    wxSize GetClientSize() const { return client; }
    bool IsShown() const { return shown; }
    wxWindow *GetParent() const { return parent; }
    void Layout() { ++layouts; }
    void CallAfter(std::function<void()> callback) { pending.push_back(callback); }
    int FromDIP(int value) const { return value; }
};
struct wxScrolledWindow : wxWindow { int extents=0; void FitInside(){++extents;} };
struct Label {
    int characters=300;
    int measured=0;
    wxSize minimum{0,-1};
    void Wrap(int width) { measured=((characters+width-1)/width)*10; }
    void InvalidateBestSize() {}
    wxSize GetBestSize() const { return {0,std::max(measured,minimum.y)}; }
    wxSize GetMinSize() const { return minimum; }
    void SetMinSize(wxSize value) { minimum=value; }
};
struct CalibrationWizardPage : wxWindow {
    std::vector<Label*> m_wrapped_labels;
    CalibrationLayout::ReflowState m_instruction_reflow;
    void queue_instruction_reflow();
};
struct CaliPresetTipsPanel : wxWindow {
    Label *m_tips_text=nullptr;
    CalibrationLayout::ReflowState m_tips_reflow;
    void queue_tips_reflow();
};
// PRODUCTION_METHODS
void drain() {
    int count=0;
    while(!pending.empty()) {
        if(++count>30)throw std::runtime_error("recursive layout did not settle");
        auto call=pending.front();pending.erase(pending.begin());call();
    }
}
void check(bool condition) { if(!condition)throw std::runtime_error("measured label height mismatch"); }
int run(const char*name,const std::function<void()> &body) {
    pending.clear();
    try { body(); std::cout<<"PASS "<<name<<'\n';return 0; }
    catch(const std::exception &e) { std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';return 1; }
}
}}
int main() {
    using namespace Slic3r::GUI;
    int failed=0;
    failed+=run("instruction narrow-wide-narrow",[]{
        Label label;CalibrationWizardPage page;wxScrolledWindow scroll;page.parent=&scroll;page.m_wrapped_labels={&label};
        page.queue_instruction_reflow();drain();check(label.minimum.y==30);
        page.client.x=300;page.queue_instruction_reflow();drain();check(label.minimum.y==10);
        page.client.x=100;page.queue_instruction_reflow();drain();check(label.minimum.y==30);check(scroll.extents==3);
    });
    failed+=run("instruction shorter content",[]{
        Label label;CalibrationWizardPage page;page.m_wrapped_labels={&label};
        page.queue_instruction_reflow();drain();check(label.minimum.y==30);
        label.characters=100;page.queue_instruction_reflow();drain();check(label.minimum.y==10);
    });
    failed+=run("advice narrow-wide-narrow",[]{
        Label label;CaliPresetTipsPanel tips;wxWindow page;wxScrolledWindow scroll;tips.parent=&page;page.parent=&scroll;tips.m_tips_text=&label;tips.client.x=140;
        tips.queue_tips_reflow();drain();check(label.minimum.y==30);
        tips.client.x=340;tips.queue_tips_reflow();drain();check(label.minimum.y==10);
        tips.client.x=140;tips.queue_tips_reflow();drain();check(label.minimum.y==30);check(scroll.extents==3);
    });
    failed+=run("advice shorter content",[]{
        Label label;CaliPresetTipsPanel tips;tips.m_tips_text=&label;tips.client.x=140;
        tips.queue_tips_reflow();drain();check(label.minimum.y==30);
        label.characters=100;tips.queue_tips_reflow();drain();check(label.minimum.y==10);
    });
    std::cout<<4-failed<<"/4 cases passed\n";return failed?1:0;
}
