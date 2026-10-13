#include "soh_mod/soh_api.h"
#include "soh_mod/client.hpp"
#include <deque>
#include <algorithm>
static_assert(sizeof(StevePose)==5296);
static_assert(offsetof(StevePose,hotbar)==56);
namespace {
steve::Client client;
struct Command { uint64_t sequence; uint32_t operation,arg0,arg1; };
std::deque<Command> commands;
uint64_t commandSequence{};
uint32_t expectedScene{};
}
extern "C" int SteveBridge_Frame(const SteveInput* input,StevePose* pose) {
    if(!input || !pose || !client.connect("default")) return 0;
    expectedScene=input->scene;
    steve::OotToMcPacket packet{};
    packet.position={input->x,input->y,input->z};
    packet.yaw=input->yaw; packet.pitch=input->pitch;
    packet.target={input->tx,input->ty,input->tz};
    packet.buttons=input->buttons; packet.locked=input->locked; packet.selected_slot=input->slot;
    packet.damage=input->damage; packet.scene=input->scene; packet.flags=input->flags; packet.health_capacity=input->health_capacity;
    packet.command_sequence=input->command_sequence; packet.command=input->command;
    packet.argument0=input->argument0; packet.argument1=input->argument1;
    packet.target_token=input->target_token; packet.target_health=input->target_health; packet.target_enemy=input->target_enemy;
    if(!packet.command_sequence && !commands.empty()) {
        const auto& command=commands.front(); packet.command_sequence=command.sequence;
        packet.command=command.operation; packet.argument0=command.arg0; packet.argument1=command.arg1;
    }
    client.frame(packet);
    while(!commands.empty() && client.state().command_ack>=commands.front().sequence) commands.pop_front();
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
    pose->block_id=state.block_id;
    pose->equipment_flags=state.reserved[0];
    if(state.backend_kind==0) pose->item_kind=state.held.id;
    for(unsigned i=0;i<9;++i) {
        const auto& item=state.hotbar[i];
        pose->hotbar[i]={item.id,item.metadata,item.count,item.durability};
    }
    for(unsigned i=0;i<36;++i) {
        const auto& item=state.inventory[i]; pose->inventory[i]={item.id,item.metadata,item.count,item.durability};
        pose->inventory_kind[i]=state.inventory_kind[i];
    }
    pose->chest={state.chest.id,state.chest.metadata,state.chest.count,state.chest.durability};
    pose->command_ack=state.command_ack; pose->command_result=state.command_result;
    pose->attack_event=state.attack_event; pose->attack_damage=state.attack_damage; pose->attack_target=state.attack_target;
    pose->tnt_event=state.tnt_event; pose->tnt_fuse=state.tnt_fuse;
    pose->tnt_x=state.tnt_position.x; pose->tnt_y=state.tnt_position.y; pose->tnt_z=state.tnt_position.z;
    pose->flight_x=state.flight_velocity.x; pose->flight_y=state.flight_velocity.y; pose->flight_z=state.flight_velocity.z;
    pose->flying=state.flying; pose->combat_cooldown=state.combat_cooldown;
    if(state.reserved[1]!=expectedScene){pose->tnt_fuse=-2;pose->flying=0;pose->flight_x=pose->flight_y=pose->flight_z=0;return p.connected;}
    for(unsigned i=0;i<8;i++) {
        const auto& tnt=state.charges[i];pose->charges[i]={tnt.position.x,tnt.position.y,tnt.position.z,tnt.fuse,tnt.token};
    }
    for(unsigned i=0;i<128;i++) {
        const auto& cell=state.cells[i];pose->cells[i]={cell.position.x,cell.position.y,cell.position.z,cell.kind,cell.token,cell.active,cell.flags};
    }
    return p.connected?1:0;
}
extern "C" int SteveBridge_PopAudio(void) {
    auto sound=client.pop_audio(); return sound?static_cast<int>(*sound):0;
}
extern "C" void SteveBridge_Disconnect(void) { client.disconnect(); commands.clear(); }
extern "C" int SteveBridge_RequestCommand(uint32_t command,uint32_t argument0,uint32_t argument1) {
    if(commands.size()>=16 || command<1 || command>4) return 0;
    commandSequence=std::max(commandSequence+1,steve::now_ns());
    commands.push_back({commandSequence,command,argument0,argument1}); return 1;
}
