#include "soh_mod/client.hpp"
#include "soh_mod/soh_api.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cmath>
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main() try {
    SteveInput input{}; StevePose pose{};
    input.scene=static_cast<std::uint32_t>(steve::now_ns()); input.flags=3;
    bool connected=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(180);
    while(std::chrono::steady_clock::now()<deadline) {
        connected=SteveBridge_Frame(&input,&pose)!=0;
        if(connected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    require(connected,"real Minecraft backend startup deadline");
    input.flags=2;
    auto run=[&](int frames) {
        for(int i=0;i<frames;++i) { SteveBridge_Frame(&input,&pose); std::this_thread::sleep_for(std::chrono::microseconds(16667)); }
    };
    auto awaitState=[&](auto condition,const char* message) {
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!condition() && std::chrono::steady_clock::now()<until) run(1);
        if (!condition()) std::cerr<<"last hand="<<pose.hand<<" left_arm="<<pose.left_arm<<" tick="<<pose.tick<<'\n';
        require(condition(),message);
    };
    run(200); // Allow vanilla's initial player join invulnerability to expire.
    require(pose.item_kind==1 && pose.hotbar[0].durability>0,"vanilla iron pickaxe initial state");
    const auto initialPick=pose.hotbar[0].durability;
    require(pose.block_id!=0,"arena stone visible before mining");
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
    require(pose.block_id==0,"mined arena block removes native collision");
    bool blockDrop=false;
    for(unsigned i=0;i<36;i++) if(pose.inventory_kind[i]==5 && pose.inventory[i].count) blockDrop=true;
    require(blockDrop,"vanilla mined block inventory pickup");
    input.buttons=steve::r; run(15);
    awaitState([&]{return pose.hand==static_cast<unsigned>(steve::Hand::block) && pose.left_arm<0;},"native offhand shield pose");
    input.buttons=0; run(8);
    input.damage=4; run(1); input.damage=0; run(8);
    std::cout<<"health after Zelda damage="<<pose.health<<'\n';
    awaitState([&]{return pose.health<=16.1f;},"Minecraft authoritative health applies Zelda damage");
    input.slot=2; input.buttons=steve::a; run(120);
    awaitState([&]{return pose.hotbar[2].count==initialFood-1;},"vanilla timed food consumption");
    input.buttons=0; run(8);
    input.slot=3; input.buttons=steve::a; run(60);
    input.buttons=0; run(20);
    awaitState([&]{return pose.hotbar[3].durability==initialBow-1 && pose.hotbar[8].count==initialArrows-1;},"vanilla bow release consumes arrow and durability");
    int sounds=0; while(SteveBridge_PopAudio()!=0) ++sounds;
    require(sounds>=3,"native audio trigger queue");
    const auto arrowsAfterShot=pose.hotbar[8].count;
    const auto bowAfterShot=pose.hotbar[3].durability;
    input.buttons=steve::a; run(60);
    SteveBridge_Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    input.buttons=0; run(20);
    awaitState([&]{return pose.hand==static_cast<unsigned>(steve::Hand::idle);},"lost frontend cancels active bow use");
    require(pose.hotbar[8].count==arrowsAfterShot && pose.hotbar[3].durability==bowAfterShot,"disconnect does not shoot or consume durability");
    std::uint64_t commandSequence=steve::now_ns();
    auto command=[&](unsigned operation,unsigned arg0,unsigned arg1) {
        input.command_sequence=++commandSequence; input.command=operation; input.argument0=arg0; input.argument1=arg1;
        awaitState([&]{return pose.command_ack==input.command_sequence;},"inventory command acknowledgement");
        require(pose.command_result==1,"Minecraft accepted inventory command");
        input.command_sequence=0;input.command=0;
    };
    require(pose.inventory_kind[4]==10 && pose.inventory_kind[5]==7 && pose.inventory_kind[6]==8 && pose.inventory_kind[7]==9,"new equipment registry kinds");
    command(1,4,10); run(5);
    require(pose.inventory_kind[10]==10 && pose.inventory[4].count==0,"36-slot inventory swap");
    command(1,4,10);command(2,7,0);run(5);
    require(pose.chest.durability>0 && pose.inventory[7].count==0,"elytra equipped in chest slot");
    input.y=3;input.flags=0;run(5);command(4,0,0);run(10);
    require(pose.flying && std::isfinite(pose.flight_z) && std::abs(pose.flight_z)>0.01f,"vanilla elytra flight velocity");
    input.flags=2;input.y=0;run(10);require(!pose.flying,"landing stops elytra glide");
    command(2,7,0);
    input.slot=4;input.target_enemy=1;input.target_token=42;input.target_health=100;input.locked=1;
    input.tx=0;input.ty=0;input.tz=2;run(90);
    const auto initialAxe=pose.hotbar[4].durability;
    const auto attackEvent=pose.attack_event;
    input.buttons=steve::b;run(5);
    awaitState([&]{return pose.attack_event>attackEvent && pose.attack_damage>0;},"vanilla netherite axe combat proxy hit");
    require(pose.attack_target==42 && pose.hotbar[4].durability<initialAxe,"authoritative axe damage and durability");
    input.buttons=0;run(8);input.buttons=steve::b;run(5);
    require(pose.attack_event>attackEvent+1,"successive axe combo hit");
    input.buttons=0;input.target_enemy=0;input.locked=0;input.slot=5;run(8);
    const auto initialTnt=pose.hotbar[5].count;
    input.buttons=steve::a;run(5);
    awaitState([&]{return pose.tnt_fuse==-1;},"vanilla TNT placement");
    require(pose.hotbar[5].count==initialTnt-1,"TNT stack consumed by placement");
    input.buttons=0;input.slot=6;run(8);const auto initialFlint=pose.hotbar[6].durability;
    input.buttons=steve::a;run(5);
    awaitState([&]{return pose.tnt_fuse>0;},"flint and steel primes real TNT");
    require(pose.hotbar[6].durability==initialFlint-1,"flint and steel durability");
    input.buttons=0;run(250);require(pose.tnt_fuse==0,"vanilla TNT fuse and detonation acknowledgement");
    std::cout<<"PASS real Minecraft 1.21.11: mining, inventory swaps, durability, shield, health, food, bow, elytra, axe combos, TNT/flint, native SoH C API and audio\n";
    SteveBridge_Disconnect();
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
