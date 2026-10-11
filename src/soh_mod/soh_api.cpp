#include "soh_mod/soh_api.h"
#include "soh_mod/client.hpp"
namespace { steve::Client client; }
extern "C" int SteveBridge_Frame(const SteveInput* input,StevePose* pose) {
    if(!input || !pose || !client.connect("default")) return 0;
    steve::OotToMcPacket packet{};
    packet.position={input->x,input->y,input->z};
    packet.yaw=input->yaw; packet.pitch=input->pitch;
    packet.target={input->tx,input->ty,input->tz};
    packet.buttons=input->buttons; packet.locked=input->locked; packet.selected_slot=input->slot;
    packet.damage=input->damage; packet.scene=input->scene; packet.flags=input->flags; packet.health_capacity=input->health_capacity;
    client.frame(packet);
    return SteveBridge_GetPose(pose);
}
extern "C" int SteveBridge_GetPose(StevePose* pose) {
    if (!pose) return 0;
    auto p=client.pose(); const auto& state=client.state();
    *pose={};
    pose->right_arm=p.right_arm; pose->left_arm=p.left_arm;
    pose->item=state.held.id; pose->hand=static_cast<uint32_t>(p.hand);
    pose->health=state.health; pose->destruction_stage=state.destruction_stage;
    pose->tick=state.tick; pose->block_event=state.block_event;
    pose->item_kind=state.backend_kind>>8; pose->selected_slot=state.selected_slot;
    if(state.backend_kind==0) pose->item_kind=state.held.id;
    for(unsigned i=0;i<9;++i) {
        const auto& item=state.hotbar[i];
        pose->hotbar[i]={item.id,item.metadata,item.count,item.durability};
    }
    return p.connected?1:0;
}
extern "C" int SteveBridge_PopAudio(void) {
    auto sound=client.pop_audio(); return sound?static_cast<int>(*sound):0;
}
extern "C" void SteveBridge_Disconnect(void) { client.disconnect(); }
