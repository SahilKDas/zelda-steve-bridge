#include "soh_mod/soh_api.h"
#include "soh_mod/client.hpp"
extern "C" int SteveBridge_Frame(const SteveInput* input,StevePose* pose) {
    static steve::Client client;
    if(!input || !pose || !client.connect("default")) return 0;
    steve::OotToMcPacket packet{};
    packet.position={input->x,input->y,input->z};
    packet.yaw=input->yaw; packet.pitch=input->pitch;
    packet.target={input->tx,input->ty,input->tz};
    packet.buttons=input->buttons; packet.locked=input->locked; packet.selected_slot=input->slot;
    client.frame(packet);
    auto p=client.pose(); const auto& state=client.state();
    *pose={p.right_arm,p.left_arm,state.held.id,static_cast<uint32_t>(p.hand),state.health,state.destruction_stage};
    return p.connected?1:0;
}
