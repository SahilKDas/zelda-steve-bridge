#include "soh_mod/terrain_clip.hpp"
#include <iostream>
#include <stdexcept>
float area(const steve::Polygon& polygon) {
    float sum=0;for(size_t i=0;i<polygon.size();i++){const auto& a=polygon[i].world;const auto& b=polygon[(i+1)%polygon.size()].world;sum+=a[0]*b[2]-a[2]*b[0];}
    return std::abs(sum)*0.5f;
}
int main() {
    try {
        steve::Polygon ground;
        for(const auto& point:std::array<std::array<float,2>,4>{{{-2,-2},{2,-2},{2,2},{-2,2}}}) {
            steve::ClipVertex vertex;vertex.world={point[0],0,point[1]};vertex.projected={point[0],0,point[1],1};vertex.uv=point;
            ground.push_back(vertex);
        }
        auto outside=steve::subtract(ground,{{-1,-1,-1},{1,1,1}});
        float remaining=0;
        for(const auto& polygon:outside){remaining+=area(polygon);for(const auto& vertex:polygon)
            if(std::abs(vertex.uv[0]-vertex.world[0])>0.0001f || std::abs(vertex.uv[1]-vertex.world[2])>0.0001f)throw std::runtime_error("texture interpolation");}
        if(std::abs(remaining-12)>0.001f)throw std::runtime_error("block-sized hole area");
        if(!steve::subtract(ground,{{-3,-1,-3},{3,1,3}}).empty())throw std::runtime_error("fully excavated polygon");
        auto intact=steve::subtract(ground,{{10,-1,10},{11,1,11}});
        if(intact.size()!=1 || std::abs(area(intact[0])-16)>0.001f)throw std::runtime_error("unrelated terrain");
        std::cout<<"PASS terrain clipping: exact hole area, UV continuity, full excavation, unrelated meshes\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
