#pragma once
#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI { namespace GameServers {
struct AreaSelection { int width=0, depth=0, x=0, z=0; };
inline AreaSelection select_chunk_area(int centre_x,int centre_z,int width,int depth,int px,int py,int qx,int qy,int viewport_w,int viewport_h,int pad)
{
    width=std::max(1,width);depth=std::max(1,depth);
    const double cell=std::min(double(viewport_w-2*pad)/width,double(viewport_h-2*pad)/depth);
    if (cell<=0) return {};
    const int ox=int((viewport_w-cell*width)/2), oy=int((viewport_h-cell*depth)/2);
    const auto chunk = [&](int p,int origin,int n,int centre) {
        const int base=int(std::floor(centre/16.0))-n/2;
        return base+std::clamp(int(std::floor((p-origin)/cell)),0,n-1);
    };
    int a=chunk(px,ox,width,centre_x), b=chunk(qx,ox,width,centre_x);
    int c=chunk(py,oy,depth,centre_z), d=chunk(qy,oy,depth,centre_z);
    if(a>b)std::swap(a,b);
    if(c>d)std::swap(c,d);
    const int w=b-a+1,h=d-c+1;
    return {w,h,std::clamp((a+w/2)*16,-29999984,29999984),std::clamp((c+h/2)*16,-29999984,29999984)};
}
}}}
