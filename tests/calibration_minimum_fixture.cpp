#include <algorithm>
#include <iostream>
#include <vector>
// PRODUCTION_MINIMUM
struct wxSize { int x,y; wxSize(int x_=-1,int y_=-1):x(x_),y(y_){} };
// This is the effective-minimum rule used by wxWindow: an explicit dimension
// replaces the computed best dimension, including when the explicit value is 0.
struct Window {
    wxSize minimum;
    int leaf_width=0;
    std::vector<Window*> children;
    void SetMinSize(wxSize size){minimum=size;}
    int BestWidth() const {
        int width=leaf_width;
        for(const auto* child:children)width=std::max(width,child->EffectiveWidth());
        return width;
    }
    int EffectiveWidth() const {return minimum.x==-1?BestWidth():minimum.x;}
};
struct Page : Window { void initialize(){ /* PAGE_INITIALIZER */ } };
struct Tips : Window { void initialize(){ /* TIPS_INITIALIZER */ } };
struct ScrollHost {
    int client_width;
    int padding;
    int virtual_width(const Window &page)const{return std::max(client_width,page.EffectiveWidth()+2*padding);}
};
int main(){
    int failed=0;
    for(int scale:{100,125,150,200}){
        Window fixed_status;fixed_status.SetMinSize({600*scale/100,-1});
        Page page;page.initialize();page.children={&fixed_status};
        ScrollHost scroll{540*scale/100,16*scale/100};
        const bool pass=scroll.virtual_width(page)>=fixed_status.EffectiveWidth()+2*scroll.padding;
        std::cout<<(pass?"PASS ":"FAIL ")<<"fixed descendant reaches outer scroll extent at "<<scale<<" percent\n";
        failed+=!pass;
    }
    Window wrapping_label;wrapping_label.leaf_width=1800;wrapping_label.SetMinSize({0,30});
    Tips tips;tips.initialize();tips.children={&wrapping_label};Page page;page.initialize();page.children={&tips};
    ScrollHost scroll{540,16};const bool pass=scroll.virtual_width(page)==scroll.client_width;
    std::cout<<(pass?"PASS ":"FAIL ")<<"bounded wrapping content remains viewport sized\n";failed+=!pass;
    std::cout<<5-failed<<"/5 cases passed\n";return failed?1:0;
}
