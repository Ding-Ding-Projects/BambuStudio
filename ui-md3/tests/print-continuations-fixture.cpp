#include "PrintSetupLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
namespace PrintSetupLayout=Slic3r::GUI::PrintSetupLayout;
static int scale=100, screen_height=600, screen_width=1000, display_owner=-1;
static int dip(int n){return (n*scale+50)/100;}
namespace MD3 { enum class Role {Primary,SurfaceContainerHighest}; namespace Metrics { struct Values{int padding;};static Values metrics{16};static const Values& active(){return metrics;} } }
struct StateColor{enum{Pressed,Hovered,Normal};template<class...T>StateColor(T...){}static int semantic(MD3::Role){return 1;}};
using wxColour=int;
struct wxSize{int x,y;wxSize(int a=-1,int b=-1):x(a),y(b){}};
static const wxSize wxDefaultSize;
constexpr int wxNOT_FOUND=-1,wxVERTICAL=1,wxEXPAND=2,wxEVT_BUTTON=3,wxID_CANCEL=4,wxYES=5,wxAPPLY=6,EVT_SECONDARY_CHECK_CONFIRM=7,EVT_SECONDARY_CHECK_CANCEL=8,EVT_UPDATE_NOZZLE=9;
constexpr int wxEVT_MOVE=10,wxEVT_DISPLAY_CHANGED=11;
struct wxMoveEvent{void Skip(){}};using wxDisplayChangedEvent=wxMoveEvent;
using wxString=string;
constexpr long wxCAPTION=1,wxCLOSE_BOX=2,wxSYSTEM_MENU=4,wxMAXIMIZE_BOX=8,wxMINIMIZE_BOX=16,wxBORDER_NONE=32;
static string _L(const char* s){return s;}
struct wxCommandEvent{int type;explicit wxCommandEvent(int t=0,int=0):type(t){}};
struct wxMouseEvent{void SetEventObject(void*){}};
struct wxSizer;
static vector<wxSizer*> live_sizers;
struct wxWindow{
 int display=0;bool shown=true,enabled=true;wxWindow* parent=nullptr;wxSizer* containing=nullptr;vector<wxWindow*> children;wxSize minimum{0,0};
 explicit wxWindow(wxWindow* p=nullptr){Reparent(p);}virtual ~wxWindow();
 void Reparent(wxWindow* p){if(parent){auto& c=parent->children;c.erase(remove(c.begin(),c.end(),this),c.end());}parent=p;if(parent)parent->children.push_back(this);}
 auto& GetChildren(){return children;}virtual wxWindow* GetParent(){return parent;}wxSizer* GetContainingSizer(){return containing;}
 bool IsShown()const{return shown;}bool IsEnabled()const{return enabled;}void Hide(){shown=false;}virtual bool Show(bool b=true){shown=b;return true;}
 virtual bool Enable(bool b=true){enabled=b;return true;}void Disable(){Enable(false);}
 void SetMinSize(wxSize s){minimum=s;}void SetMaxSize(wxSize){}virtual wxSize GetBestSize(){return minimum;}wxSize GetMinSize(){return minimum;}
 int GetCharHeight(){return dip(20);}int FromDIP(int n){return dip(n);}
};
struct wxSizerItem{wxWindow* window;int proportion,flags,border;wxSizer* nested=nullptr;int GetProportion()const{return proportion;}int GetFlag()const{return flags;}int GetBorder()const{return border;}};
struct wxSizer{
 vector<wxSizerItem*> items;function<wxSize()> measure;wxSizer* parent_sizer=nullptr;
 explicit wxSizer(function<wxSize()> f={}):measure(f){live_sizers.push_back(this);}
 virtual ~wxSizer(){for(auto* i:items){if(i->nested){i->nested->parent_sizer=nullptr;delete i->nested;}else if(i->window->containing==this)i->window->containing=nullptr;delete i;}live_sizers.erase(remove(live_sizers.begin(),live_sizers.end(),this),live_sizers.end());}
 auto& GetChildren(){return items;}wxSizerItem* GetItem(wxWindow* w){for(auto* i:items)if(i->window==w)return i;abort();}
 void Add(wxWindow* w,int p=0,int f=0,int b=0){Insert(items.size(),w,p,f,b);}
 void Add(wxSizer* s,int p=0,int f=0,int b=0){if(s->parent_sizer){fprintf(stderr,"double-owned nested sizer\n");abort();}s->parent_sizer=this;items.push_back(new wxSizerItem{nullptr,p,f,b,s});}
 void Insert(size_t n,wxWindow* w,int p,int f,int b=0){if(w->containing){fprintf(stderr,"double-owned window\n");abort();}items.insert(items.begin()+n,new wxSizerItem{w,p,f,b});w->containing=this;}
 void Detach(wxWindow* w){auto it=find_if(items.begin(),items.end(),[w](auto* i){return i->window==w;});if(it==items.end())abort();w->containing=nullptr;delete *it;items.erase(it);}
 wxSize CalcMin(){if(measure)return measure();wxSize out{0,0};for(auto* i:items){if(i->nested){const auto sz=i->nested->CalcMin();out.x=max(out.x,sz.x);out.y+=sz.y;}else if(i->window->shown){out.x=max(out.x,i->window->minimum.x);out.y+=i->window->minimum.y;}}return out;}
};
struct wxBoxSizer:wxSizer{explicit wxBoxSizer(int){}};
wxWindow::~wxWindow(){if(containing)containing->Detach(this);while(!children.empty())delete children.back();Reparent(nullptr);}
struct wxPanel:wxWindow{using wxWindow::wxWindow;};
struct Label:wxWindow{
 int chars=2000,width=400,wraps=0;using wxWindow::wxWindow;
 void SetMinSize(wxSize s){minimum=s;width=s.x;}void Wrap(int n){width=n;++wraps;}
 int height()const{return ((chars*dip(8)+width-1)/width)*dip(20);}
};
struct Button:wxWindow{
 enum class Variant{Outlined};enum class Size{Medium};function<void(wxCommandEvent&)> click;
 Button(wxWindow* p,const string&):wxWindow(p){minimum={dip(80),dip(40)};}
 void SetVariant(Variant){}void SetButtonSize(Size){}void SetToolTip(string){}void Rescale(){minimum={dip(80),dip(40)};}
 template<class F>void Bind(int,F f){click=f;}template<class T>void SetBackgroundColor(T){}template<class T>void SetBorderColor(T){}
 void activate(){wxCommandEvent e;if(click)click(e);}
};
struct wxScrolledWindow:wxWindow{
 Label* label;wxSizer* sizer;int allocated=480,allocated_y=0,virtual_height=0,inside=0,rate_x=0;
 explicit wxScrolledWindow(wxWindow* p):wxWindow(p),label(new Label(this)),sizer(new wxSizer([this]{return wxSize(label->width+2*dip(MD3::Metrics::active().padding),label->height()+2*dip(MD3::Metrics::active().padding));})){minimum={480,0};}
 ~wxScrolledWindow(){delete sizer;}
 wxSizer* GetSizer(){return sizer;}wxSize GetClientSize(){return {allocated-(virtual_height>allocated_y?dip(16):0),allocated_y};}
 void FitInside(){virtual_height=sizer->CalcMin().y;++inside;}void SetScrollRate(int x,int){rate_x=x;}
};
struct wxDialog:wxWindow{
 wxScrolledWindow* body;wxSizer* root;wxWindow owner;int fits=0,closed=0,events=0,result=0;wxSize client{528,100};function<void()> on_fit;wxWindow* caption=nullptr;long style=wxCAPTION|wxCLOSE_BOX;
 wxDialog():body(new wxScrolledWindow(this)),root(new wxSizer([this]{return wxSize(body->minimum.x+dip(48),body->minimum.y+dip(170));})){shown=false;owner.display=1;root->Add(body);}
 ~wxDialog(){delete root;root=nullptr;}
 map<int,function<void(wxMoveEvent&)>> bindings;
 template<class F>void Bind(int type,F f){bindings[type]=f;}void emit(int type){wxMoveEvent e;if(bindings.count(type))bindings[type](e);}
 wxSizer* GetSizer(){return root;}void SetSizer(wxSizer* s,bool destroy=true){if(destroy)delete root;root=s;}
 void Fit(){client=root->CalcMin();++fits;if(fits==1&&on_fit)on_fit();}
 void Layout(){if(root->measure||any_of(root->items.begin(),root->items.end(),[](auto* i){return i->nested!=nullptr;})){body->allocated=client.x-dip(48);body->allocated_y=body->minimum.y;}else{int fixed=0;for(auto* i:root->items)if(!i->proportion&&i->window->shown)fixed+=i->window->minimum.y;body->allocated=client.x;body->allocated_y=max(0,client.y-fixed);}}
 wxWindow* GetParent()override{return &owner;}wxSize GetSize(){return {client.x,client.y+((style&wxCAPTION)?dip(20):0)};}wxSize GetClientSize(){return client;}void SetClientSize(wxSize s){client=s;}void SetClientSize(int x,int y){client={x,y};}
 long GetWindowStyleFlag(){return style;}void SetWindowStyleFlag(long s){style=s;}wxString GetTitle(){return "sending failed";}void CentreOnParent(){}
 wxDialog* GetEventHandler(){return this;}void ProcessEvent(wxCommandEvent& e){++events;result=e.type;}int GetId(){return 42;}void EndModal(int r){++closed;result=r;shown=false;}
};
struct MD3DialogCaption:wxPanel{
 bool m_follow_title=false;
 MD3DialogCaption(wxDialog* d,const wxString&):wxPanel(d){minimum={dip(80),dip(44)};d->caption=this;}
 static void Adopt(wxDialog*,const wxString& title=wxString());static void FinishChrome(wxDialog*){}
};
struct App{void UpdateDlgDarkUI(wxDialog*){}};static App& wxGetApp(){static App app;return app;}
using MD3Dialog=wxDialog;using DPIDialog=wxDialog;
struct wxDisplay{explicit wxDisplay(int){}static int GetFromWindow(wxWindow* w){display_owner=w->display;return 0;}struct Area{int width,height;};Area GetClientArea()const{return {dip(screen_width),dip(screen_height)};}};
// PRODUCTION_BUTTON
void bind_continuation_refresh(wxDialog*,const function<void()>&);
// PRODUCTION_STATE
struct ConfirmBeforeSendDialog:wxDialog{
 ConfirmBeforeSendDialog();
 bool m_fitting_content=false,m_presentation_available=true;ContinuationDisclosureLayout m_disclosure;
 wxScrolledWindow* m_vebview_release_note=body;Button* m_button_ok=new ContinuationButton(this,"Confirm");Button* m_button_cancel=new Button(this,"Cancel");Button* m_button_update_nozzle=new ContinuationButton(this,"Nozzle");
 void fit_content();bool Show(bool);void disable_button_ok();void enable_button_ok();void on_hide(){EndModal(wxID_CANCEL);}void click_ok(wxMouseEvent&);void click_nozzle(wxMouseEvent&);
};
struct InputIpAddressDialog:wxDialog{
 InputIpAddressDialog();
 bool m_fitting_content=false,m_presentation_available=true;ContinuationDisclosureLayout m_disclosure;
 wxScrolledWindow* m_body=body;Button* m_button_ok=new ContinuationButton(this,"Connect");int attempts=0;
 void fit_content();bool Show(bool);void OnHeaderClose(){EndModal(wxID_CANCEL);}bool isIp(string);
 void on_ok(wxMouseEvent&);void on_send_retry();
};
struct SendFailedConfirm:wxDialog{
 SendFailedConfirm();
 bool m_fitting_content=false,m_presentation_available=true;ContinuationDisclosureLayout m_disclosure;
 wxScrolledWindow* m_body=body;Button* m_button_retry=new ContinuationButton(this,"Retry");Button* m_button_input=new ContinuationButton(this,"Reconnect");
 void fit_content();bool Show(bool);void click_retry(wxMouseEvent&);void click_input(wxMouseEvent&);
};
// PRODUCTION_METHODS
int main(){
 int checks=0;auto require=[&](bool ok,const char* text){++checks;if(!ok)fprintf(stderr,"FAILED: %s\n",text);return ok;};wxMouseEvent event;
 screen_height=100;const auto before_caption=live_sizers.size();
 {
  SendFailedConfirm d;
  auto* adopted=d.caption->GetContainingSizer();
  if(!require(d.m_disclosure.normal_sizer==adopted&&d.GetSizer()!=adopted,"constructor saves the adopted caption root before readback"))return 1;
  if(!require(d.m_disclosure.body_holder->parent_sizer==adopted,"body holder remains nested under caption root"))return 1;
  screen_height=1000;d.Show(true);
  if(!require(d.GetSizer()==adopted&&d.caption->GetParent()==&d&&d.caption->IsShown(),"growth restores the caption and its complete root"))return 1;
  screen_height=100;d.Show(false);d.Show(true);
  if(!require(d.m_disclosure.normal_sizer==adopted&&d.GetSizer()->parent_sizer==nullptr,"reopen keeps readback separate from the caption tree"))return 1;
  d.m_disclosure.close->activate();
  if(!require(d.result==wxID_CANCEL&&d.closed==1,"caption-bearing readback still cancels"))return 1;
 }
 if(!require(live_sizers.size()==before_caption,"both nested layout trees are released in the fixture"))return 1;
 for(int work:{100,0,220}){
  screen_height=work;screen_width=work==0?0:1000;ConfirmBeforeSendDialog d;d.fit_content();
  if(!require(!d.m_presentation_available&&!d.m_button_ok->IsEnabled(),"exhausted disclosure disables confirmation"))return 1;
  if(!require(d.m_disclosure.normal_sizer&&d.body->allocated_y>0&&d.body->virtual_height>d.body->allocated_y,"actual disclosure retains a scroll viewport"))return 1;
  if(!require(d.m_disclosure.close->IsShown()&&d.m_disclosure.close->IsEnabled(),"readback has available close control"))return 1;
  d.click_ok(event);d.click_nozzle(event);
  if(!require(d.events==0&&d.closed==0,"direct continuation events cannot authorize readback"))return 1;
  d.disable_button_ok();d.m_button_update_nozzle->Hide();d.m_button_update_nozzle->Disable();screen_height=1000;screen_width=1000;d.shown=true;d.emit(wxEVT_DISPLAY_CHANGED);
  if(!require(d.m_presentation_available&&!d.m_button_ok->IsEnabled()&&!d.m_button_update_nozzle->IsShown()&&!d.m_button_update_nozzle->IsEnabled(),"growth preserves caller disable and visibility"))return 1;
  d.enable_button_ok();
  if(!require(d.m_button_ok->IsEnabled(),"caller may reenable only with readable presentation"))return 1;
  screen_height=work;screen_width=work==0?0:1000;d.emit(wxEVT_MOVE);
  if(!require(!d.m_presentation_available&&d.m_disclosure.normal_sizer,"move refresh reenters readback on smaller display"))return 1;
  d.Show(false);d.Show(true);
  if(!require(!d.m_presentation_available&&d.m_disclosure.normal_sizer,"cached reopen reenters readback"))return 1;
  d.enable_button_ok();
  if(!require(!d.m_button_ok->IsEnabled(),"caller enable cannot override unavailable disclosure"))return 1;
  d.m_disclosure.close->activate();
  if(!require(d.events==1&&d.closed==1,"actual readback close callback cancels without confirm"))return 1;
 }
 screen_width=1000;screen_height=100;InputIpAddressDialog input;input.fit_content();input.on_ok(event);input.on_send_retry();
 if(!require(!input.m_button_ok->IsEnabled()&&input.attempts==0,"address entry cannot connect without disclosure"))return 1;
 input.m_button_ok->Enable(true);
 if(!require(!input.m_button_ok->IsEnabled(),"validation enable cannot override unavailable disclosure"))return 1;
 input.m_button_ok->Enable(false);screen_height=1000;input.Show(true);
 if(!require(input.m_presentation_available&&!input.m_button_ok->IsEnabled(),"connection validation disable survives restoration"))return 1;
 input.m_button_ok->Enable(true);input.on_ok(event);
 if(!require(input.attempts==1,"valid caller capability restores after growth"))return 1;
 screen_height=0;input.Show(false);input.Show(true);input.m_disclosure.close->activate();
 if(!require(!input.m_presentation_available&&input.closed==1,"address readback close invokes existing teardown"))return 1;
 screen_height=100;SendFailedConfirm failure;failure.fit_content();failure.click_retry(event);failure.click_input(event);
 if(!require(!failure.m_button_retry->IsEnabled()&&!failure.m_button_input->IsEnabled()&&!failure.closed,"failed-send choices require disclosure"))return 1;
 screen_height=1000;failure.Show(true);failure.click_retry(event);
 if(!require(failure.result==wxYES&&failure.closed==1,"retry result survives restoration"))return 1;
 screen_height=0;failure.Show(true);failure.m_disclosure.close->activate();
 if(!require(failure.result==wxID_CANCEL&&failure.closed==2,"failed-send readback keeps cancellation"))return 1;
 for(int pct:{100,125,150,200})for(int padding:{10,16})for(int work:{360,600,1000}){
  scale=pct;screen_height=work;MD3::Metrics::metrics.padding=padding;ConfirmBeforeSendDialog d;
  d.on_fit=[&]{d.fit_content();};d.fit_content();
  if(!require(d.GetSize().y<=dip(work)-2*dip(12),"measured chrome and body fit display"))return 1;
  if(!require(d.body->virtual_height>d.body->allocated_y,"full message stays scrollable"))return 1;
  if(!require(d.body->label->width<=d.body->GetClientSize().x-2*dip(padding),"text fits scrollbar-adjusted width"))return 1;
  if(!require(d.fits==3&&!d.m_fitting_content&&d.body->inside==2,"nested fit does not recurse"))return 1;
  if(!require(display_owner==1&&d.m_presentation_available,"hidden owner uses readable parent display"))return 1;
 }
 scale=100;
 for(auto value:{"127.0.0.1","192.168.50.2","0.0.0.0","255.255.255.255"})if(!require(input.isIp(value),"accepted address stays accepted"))return 1;
 for(auto value:{"256.0.1.1","1.2.3.256","1-2.3.4","1.2.3.4extra"})if(!require(!input.isIp(value),"rejected address stays rejected"))return 1;
 printf("%d continuation owner assertions passed\n",checks);
}
