#include "soh_mod/soh_api.h"
#include "soh_mod/terrain_clip.hpp"
#include <ship/Context.h>
#include <fast/Fast3dWindow.h>
#include <mutex>
#include <atomic>
namespace {
std::mutex cutMutex;
std::vector<steve::CutBox> cuts;
std::atomic<bool> hasCuts{};
std::atomic<int> clippedTriangles{};
bool clip(const Fast::LoadedVertex& a,const Fast::LoadedVertex& b,const Fast::LoadedVertex& c,
          std::vector<std::array<Fast::LoadedVertex,3>>& output) {
    if(!hasCuts.load(std::memory_order_relaxed) || !a.terrain || !b.terrain || !c.terrain)return false;
    const Fast::LoadedVertex* originals[]={&a,&b,&c};
    steve::Polygon triangle;
    for(auto* source:originals)triangle.push_back({{source->world[0],source->world[1],source->world[2]},
        {source->x,source->y,source->z,source->w},{source->u,source->v},
        {float(source->color.r),float(source->color.g),float(source->color.b),float(source->color.a)}});
    std::vector<steve::CutBox> boxes;
    {std::lock_guard lock(cutMutex);for(const auto& box:cuts)if(steve::overlaps(triangle,box))boxes.push_back(box);}
    if(boxes.empty())return false;
    clippedTriangles.fetch_add(1,std::memory_order_relaxed);
    std::vector<steve::Polygon> polygons{triangle};
    for(const auto& box:boxes) {
        std::vector<steve::Polygon> remaining;
        for(auto& polygon:polygons){auto pieces=steve::subtract(std::move(polygon),box);for(auto& piece:pieces)remaining.push_back(std::move(piece));}
        polygons=std::move(remaining);
    }
    auto convert=[](const steve::ClipVertex& source) {
        Fast::LoadedVertex vertex{};
        vertex.x=source.projected[0];vertex.y=source.projected[1];vertex.z=source.projected[2];vertex.w=source.projected[3];
        vertex.u=source.uv[0];vertex.v=source.uv[1];
        vertex.color={static_cast<uint8_t>(std::clamp(source.color[0],0.f,255.f)),static_cast<uint8_t>(std::clamp(source.color[1],0.f,255.f)),
            static_cast<uint8_t>(std::clamp(source.color[2],0.f,255.f)),static_cast<uint8_t>(std::clamp(source.color[3],0.f,255.f))};
        // Clipped triangles stay within the original triangle; normal RSP culling follows.
        return vertex;
    };
    for(const auto& polygon:polygons)for(size_t i=1;i+1<polygon.size();i++)output.push_back({convert(polygon[0]),convert(polygon[i]),convert(polygon[i+1])});
    return true;
}
}
extern "C" void SteveTerrain_Sync(const StevePose* pose) {
    auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    static std::weak_ptr<Fast::Interpreter> installed;
    if(window)if(auto interpreter=window->GetInterpreterWeak().lock())if(installed.lock()!=interpreter){interpreter->triangleClipper=clip;installed=interpreter;}
    std::lock_guard lock(cutMutex);cuts.clear();
    if(pose)for(const auto& cell:pose->cells)if(cell.active && (cell.flags&1))cuts.push_back({
        {cell.x*20-10.05f,cell.y*20-10.05f,cell.z*20-10.05f},{cell.x*20+10.05f,cell.y*20+10.05f,cell.z*20+10.05f}});
    hasCuts.store(!cuts.empty(),std::memory_order_relaxed);
}
extern "C" int SteveTerrain_ClippedTriangles(void) {return clippedTriangles.load(std::memory_order_relaxed);}
extern "C" int SteveTerrain_Contains(float x,float y,float z) {
    if(!hasCuts.load(std::memory_order_relaxed))return 0;
    std::lock_guard lock(cutMutex);
    for(const auto& box:cuts)if(x>=box.min[0]&&x<=box.max[0]&&y>=box.min[1]&&y<=box.max[1]&&z>=box.min[2]&&z<=box.max[2])return 1;
    return 0;
}
