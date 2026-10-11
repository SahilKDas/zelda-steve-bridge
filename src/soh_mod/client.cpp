#include "soh_mod/client.hpp"
#include <algorithm>
#include <cmath>
namespace steve {
bool Client::connect(const std::string& session) {
    if(mapping) return true;
    if(now_ns()<retry_at) return false;
    retry_at=now_ns()+500'000'000;
    auto result=Mapping::open(session,false);
    if(!result) return false;
    mapping.emplace(std::move(*result));
    return true;
}
void Client::disconnect() { mapping.reset(); previous={}; current={}; retry_at=0; last_buttons=0; pending_pressed=0; pending_damage=0; audio_read=audio_write=0; }
std::optional<Sound> Client::pop_audio() {
    if(audio_read==audio_write) return std::nullopt;
    return sounds[audio_read++%sounds.size()];
}
bool Client::frame(OotToMcPacket input) {
    if(!mapping) return false;
    input.sequence=++sequence;
    input.time_ns=now_ns();
    pending_pressed |= input.buttons & ~last_buttons;
    if(std::isfinite(input.damage) && input.damage>0) pending_damage=std::min(20.0f,pending_damage+input.damage);
    input.damage=pending_damage;
    last_buttons=input.buttons;
    input.pressed_buttons=pending_pressed;
    auto& s=mapping->get();
    if(s.ready.load(std::memory_order_acquire)!=magic || now_ns()-s.heartbeat_ns.load(std::memory_order_acquire)>500'000'000) {
        disconnect(); return false;
    }
    bool sent=false;
    if(auto* p=s.to_backend.reserve()) { *p=input; s.to_backend.publish(); sent=true; pending_pressed=0; pending_damage=0; }
    while(auto* p=s.to_frontend.peek()) {
        previous=current; current=*p;
        for(std::uint32_t i=0;i<std::min(p->audio_count,8u);++i) {
            if(audio_write-audio_read<sounds.size()) sounds[audio_write++%sounds.size()]=p->audio[i];
            else ++dropped_audio;
        }
        s.to_frontend.consume();
    }
    // Reopen after a stalled/crashed bridge. Windows names live until all handles close.
    if(current.tick && now_ns()-current.time_ns>2'000'000'000) disconnect();
    return sent;
}
Pose Client::pose(std::uint64_t time) const {
    if(!current.tick || time<current.time_ns || time-current.time_ns>250'000'000) return {};
    // Render one MC tick behind, interpolating continuous pose only. Inventory is discrete.
    auto render_time=time>50'000'000?time-50'000'000:0;
    float alpha=1;
    if(current.time_ns>previous.time_ns) {
        const auto elapsed=render_time>previous.time_ns?render_time-previous.time_ns:0;
        alpha=std::clamp(static_cast<float>(elapsed)/static_cast<float>(current.time_ns-previous.time_ns),0.0f,1.0f);
    }
    float swing=current.swing;
    if(previous.hand==current.hand && current.swing>=previous.swing) swing=std::lerp(previous.swing,current.swing,alpha);
    float right=0,left=0;
    if(current.hand==Hand::mine_swing || current.hand==Hand::attack) right=-std::sin(swing*3.14159265f)*1.5f;
    if(current.hand==Hand::block) left=-1.2f;
    if(current.hand==Hand::consume || current.hand==Hand::bow) right=-1.3f;
    return {current.hand,swing,right,left,true};
}
}
