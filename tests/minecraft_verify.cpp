#include "soh_mod/client.hpp"
#include "soh_mod/soh_api.h"
#include <iostream>
#include <stdexcept>
#include <thread>
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main() try {
    SteveInput input{}; StevePose pose{};
    input.scene=static_cast<std::uint32_t>(steve::now_ns()); input.flags=1;
    bool connected=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(180);
    while(std::chrono::steady_clock::now()<deadline) {
        connected=SteveBridge_Frame(&input,&pose)!=0;
        if(connected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    require(connected,"real Minecraft backend startup deadline");
    input.flags=0;
    auto run=[&](int frames) {
        for(int i=0;i<frames;++i) { SteveBridge_Frame(&input,&pose); std::this_thread::sleep_for(std::chrono::microseconds(16667)); }
    };
    run(200); // Allow vanilla's initial player join invulnerability to expire.
    require(pose.item_kind==1 && pose.hotbar[0].durability>0,"vanilla iron pickaxe initial state");
    const auto initialPick=pose.hotbar[0].durability;
    const auto initialFood=pose.hotbar[2].count;
    const auto initialBow=pose.hotbar[3].durability;
    const auto initialArrows=pose.hotbar[8].count;
    std::cout<<"Minecraft registry pickaxe ID="<<pose.item<<" durability="<<pose.hotbar[0].durability<<'\n';
    auto event=pose.block_event;
    input.buttons=steve::b;
    for(int i=0;i<240 && pose.block_event==event;++i) run(1);
    require(pose.block_event>event,"vanilla stone mining acknowledged");
    require(pose.hotbar[0].durability==initialPick-1,"vanilla mining durability loss");
    input.buttons=0; run(15);
    bool blockDrop=false;
    for(const auto& item:pose.hotbar) if(item.id && item.id!=pose.hotbar[0].id && item.id!=pose.hotbar[1].id && item.id!=pose.hotbar[2].id && item.id!=pose.hotbar[3].id && item.id!=pose.hotbar[8].id) blockDrop=true;
    require(blockDrop,"vanilla mined block inventory pickup");
    input.buttons=steve::r; run(15);
    require(pose.hand==static_cast<unsigned>(steve::Hand::block) && pose.left_arm<0,"native offhand shield pose");
    input.buttons=0; run(8);
    input.damage=4; run(1); input.damage=0; run(8);
    std::cout<<"health after Zelda damage="<<pose.health<<'\n';
    require(pose.health<=16.1f,"Minecraft authoritative health applies Zelda damage");
    input.slot=2; input.buttons=steve::a; run(120);
    require(pose.hotbar[2].count==initialFood-1,"vanilla timed food consumption");
    input.buttons=0; run(8);
    input.slot=3; input.buttons=steve::a; run(60);
    input.buttons=0; run(20);
    require(pose.hotbar[3].durability==initialBow-1 && pose.hotbar[8].count==initialArrows-1,"vanilla bow release consumes arrow and durability");
    int sounds=0; while(SteveBridge_PopAudio()!=0) ++sounds;
    require(sounds>=3,"native audio trigger queue");
    std::cout<<"PASS real Minecraft 1.21.11: mining, drops, durability, shield, health, food, bow, native SoH C API and audio\n";
    SteveBridge_Disconnect();
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
