#pragma once
#include "bridge/protocol.hpp"
#include <algorithm>
#include <cmath>
namespace steve {
// Deterministic Minecraft-style test world: one regenerating block in reach.
// Item IDs are bridge-local, not version-specific Minecraft registry numbers.
class World {
    McToOotPacket state{};
    std::uint32_t previous_buttons{}, progress{};
public:
    World() {
        state.hotbar[0]={1,0,1,250}; // pickaxe
        state.hotbar[1]={2,0,1,250}; // sword
        state.hotbar[2]={3,0,8,0};   // food
        state.hotbar[3]={4,0,1,384}; // bow
    }
    McToOotPacket tick(const OotToMcPacket& input) {
        ++state.tick;
        state.time_ns=now_ns();
        state.input_sequence=input.sequence;
        state.selected_slot=std::min(input.selected_slot,8u);
        auto& held=state.hotbar[state.selected_slot];
        state.audio_count=0;
        state.audio.fill(Sound::none);
        state.hand=Hand::idle;
        state.destruction_stage=-1;
        state.swing=0;
        const bool stale=state.time_ns<input.time_ns || state.time_ns-input.time_ns>250'000'000;
        auto buttons=stale?0u:input.buttons;
        auto pressed=buttons & ~previous_buttons;
        previous_buttons=buttons;
        auto sound=[&](Sound value) { state.audio[state.audio_count++]=value; };
        if(buttons&r) state.hand=Hand::block;
        else if((buttons&b) && held.count && held.durability) {
            state.hand=held.id==1?Hand::mine_swing:Hand::attack;
            state.swing=static_cast<float>(state.tick%4)/4.0f;
            const float dx=input.target.x-input.position.x;
            const float dy=input.target.y-input.position.y;
            const float dz=input.target.z-input.position.z;
            if(held.id==1 && input.locked && std::isfinite(dx+dy+dz) && dx*dx+dy*dy+dz*dz<=25.0f) {
                state.destruction_stage=static_cast<std::int32_t>(progress++);
                if(progress==10) {
                    progress=0; --held.durability;
                    auto& blocks=state.hotbar[4]; blocks.id=5; blocks.count=std::min(blocks.count+1,64u);
                    sound(Sound::broken_block);
                }
            } else { progress=0; if(pressed&b) { --held.durability; sound(Sound::hit); } }
        } else if((pressed&a) && held.count) {
            if(held.id==3) { state.hand=Hand::consume; --held.count; state.health=std::min(state.health+4,20.0f); sound(Sound::eat); }
            if(held.id==4 && held.durability) { state.hand=Hand::bow; --held.durability; sound(Sound::shoot); }
        } else progress=0;
        state.held=held;
        return state;
    }
};
}
