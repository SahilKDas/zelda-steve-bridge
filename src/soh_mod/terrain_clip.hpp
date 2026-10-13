#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
namespace steve {
struct ClipVertex {
    std::array<float,3> world{};
    std::array<float,4> projected{};
    std::array<float,2> uv{};
    std::array<float,4> color{};
};
struct CutBox {std::array<float,3> min{},max{};};
using Polygon=std::vector<ClipVertex>;
inline ClipVertex interpolate(const ClipVertex& a,const ClipVertex& b,float t) {
    ClipVertex result;
    for(unsigned i=0;i<3;i++)result.world[i]=std::lerp(a.world[i],b.world[i],t);
    for(unsigned i=0;i<4;i++){result.projected[i]=std::lerp(a.projected[i],b.projected[i],t);result.color[i]=std::lerp(a.color[i],b.color[i],t);}
    for(unsigned i=0;i<2;i++)result.uv[i]=std::lerp(a.uv[i],b.uv[i],t);
    return result;
}
inline Polygon halfspace(const Polygon& polygon,unsigned axis,float plane,bool greater) {
    Polygon result;
    if(polygon.empty())return result;
    auto previous=polygon.back();
    bool previousInside=greater?previous.world[axis]>=plane:previous.world[axis]<=plane;
    for(const auto& current:polygon) {
        bool inside=greater?current.world[axis]>=plane:current.world[axis]<=plane;
        if(inside!=previousInside) {
            float denominator=current.world[axis]-previous.world[axis];
            if(std::abs(denominator)>1e-6f)result.push_back(interpolate(previous,current,(plane-previous.world[axis])/denominator));
        }
        if(inside)result.push_back(current);
        previous=current;previousInside=inside;
    }
    return result;
}
inline bool overlaps(const Polygon& polygon,const CutBox& box) {
    for(unsigned axis=0;axis<3;axis++) {
        float low=polygon.front().world[axis],high=low;
        for(const auto& vertex:polygon){low=std::min(low,vertex.world[axis]);high=std::max(high,vertex.world[axis]);}
        if(high<box.min[axis] || low>box.max[axis])return false;
    }
    return true;
}
// Convex subtraction: retain each outside slab and continue with the inside remainder.
inline std::vector<Polygon> subtract(Polygon polygon,const CutBox& box) {
    std::vector<Polygon> result;
    if(!overlaps(polygon,box)){result.push_back(std::move(polygon));return result;}
    for(unsigned axis=0;axis<3 && polygon.size()>=3;axis++) {
        auto outside=halfspace(polygon,axis,box.min[axis],false);
        if(outside.size()>=3)result.push_back(std::move(outside));
        polygon=halfspace(polygon,axis,box.min[axis],true);
        outside=halfspace(polygon,axis,box.max[axis],true);
        if(outside.size()>=3)result.push_back(std::move(outside));
        polygon=halfspace(polygon,axis,box.max[axis],false);
    }
    return result;
}
}
