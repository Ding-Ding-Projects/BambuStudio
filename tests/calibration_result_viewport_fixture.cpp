#include "src/slic3r/GUI/CalibrationLayout.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace Slic3r { namespace GUI {
struct wxSize { int x,y; wxSize(int x_=0,int y_=0):x(x_),y(y_){} };
constexpr int wxID_ANY=0,wxDefaultPosition=0,wxDefaultSize=0,wxTAB_TRAVERSAL=1,wxHSCROLL=2,wxBORDER_NONE=4;
constexpr int wxSHOW_SB_DEFAULT=0,wxSHOW_SB_NEVER=1,wxEVT_SIZE=1,wxEVT_SHOW=2,wxEVT_DPI_CHANGED=3;
struct wxSizeEvent{void Skip(){}};struct wxShowEvent{void Skip(){}bool IsShown(){return true;}};struct wxDPIChangedEvent{void Skip(){}};
std::vector<std::function<void()>> pending;
struct Sizer { wxSize contents{1120,80};wxSize CalcMin()const{return contents;} };
struct wxWindow {
    virtual ~wxWindow()=default;
    wxWindow *parent=nullptr;wxSize client{400,100},minimum{-1,-1},virtual_size;
    std::vector<wxWindow*> children;
    int EffectiveWidth()const{if(minimum.x!=-1)return minimum.x;int width=sizer?sizer->contents.x:0;for(auto*child:children)width=std::max(width,child->EffectiveWidth());return width;}
    Sizer *sizer=nullptr;bool shown=true;int scale=100,layouts=0;std::function<void()> layout_callback;
    wxSize GetClientSize()const{return client;}wxSize GetMinSize()const{return minimum;}
    wxWindow *GetParent()const{return parent;}bool IsShown()const{return shown;}Sizer *GetSizer(){return sizer;}
    void SetMinSize(wxSize value){minimum=value;}void SetVirtualSize(wxSize value){virtual_size=value;}
    void Layout(){++layouts;if(layout_callback)layout_callback();}
    int FromDIP(int value)const{return value*scale/100;}
    template<class Callback>void Bind(int,Callback){}
    void CallAfter(std::function<void()> callback){pending.push_back(callback);}
};
struct wxScrolledWindow:wxWindow{int extents=0;void FitInside(){++extents;}};
struct MD3ScrolledWindow:wxScrolledWindow{
    int horizontal_rate=0,vertical_rate=0;
    MD3ScrolledWindow(wxWindow *owner,int,int,int,int){parent=owner;}
    void SetScrollRate(int x,int y){horizontal_rate=x;vertical_rate=y;}
    void ShowScrollbars(int,int){}
    static int BarThickness(const wxWindow *window){return window->FromDIP(12);}
};
// PRODUCTION_VIEWPORT
void drain(){int count=0;while(!pending.empty()){if(++count>30)throw std::runtime_error("layout did not settle");auto call=pending.front();pending.erase(pending.begin());call();}}
void check(bool value){if(!value)throw std::runtime_error("viewport geometry mismatch");}
struct Setup{
    wxScrolledWindow outer;wxWindow page,panel;Sizer table;CalibrationResultViewport view{&panel};
    Setup(){panel.parent=&page;page.parent=&outer;view.sizer=&table;page.children={&panel};panel.children={&view};}
    void refresh(){view.QueueExtent();drain();}
};
int run(const char*name,const std::function<void()> &fn){pending.clear();try{fn();std::cout<<"PASS "<<name<<'\n';return 0;}catch(const std::exception&e){std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';return 1;}}
}}
int main(){using namespace Slic3r::GUI;int failed=0;
 failed+=run("wide table keeps local overflow and full height",[]{Setup s;s.refresh();check(s.page.EffectiveWidth()<=s.view.client.x&&s.view.minimum.y==92);check(s.view.virtual_size.x==1120&&s.view.virtual_size.y==80);check(s.view.horizontal_rate==12&&s.view.vertical_rate==0);});
 failed+=run("narrow-wide-narrow removes then restores bar space",[]{Setup s;s.refresh();s.view.client.x=1200;s.refresh();check(s.view.minimum.y==80&&s.view.virtual_size.x==1200);s.view.client.x=400;s.refresh();check(s.view.minimum.y==92);});
 failed+=run("shorter content shrinks without old minimum clamp",[]{Setup s;s.refresh();s.table.contents={800,40};s.refresh();check(s.view.minimum.y==52&&s.view.virtual_size.x==800);});
 failed+=run("DPI refresh measures content and scrollbar at current scale",[]{Setup s;s.view.scale=200;s.view.client.x=800;s.table.contents={2240,160};s.refresh();check(s.view.minimum.y==184&&s.view.horizontal_rate==24);});
 failed+=run("hidden and zero viewport resume after show",[]{Setup s;s.view.shown=false;s.refresh();check(s.outer.extents==0);s.view.shown=true;s.view.client.x=0;s.refresh();check(s.outer.extents==0);s.view.client.x=400;s.refresh();check(s.outer.extents==1);});
 failed+=run("ancestor refresh and changed width settle without recursion",[]{Setup s;s.view.layout_callback=[&]{s.view.QueueExtent();};s.panel.layout_callback=[&]{s.view.client.x=1200;};s.refresh();check(s.view.minimum.y==80&&s.outer.extents==2&&s.page.layouts==2);});
 std::cout<<6-failed<<"/6 cases passed\n";return failed?1:0;
}
