#include "soh_mod/client.hpp"
#include "soh_mod/soh_api.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cmath>
#include <algorithm>
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() try {
 SteveInput input{};StevePose pose{};input.scene=static_cast<uint32_t>(steve::now_ns());input.flags=3;
 auto run=[&](int frames){for(int i=0;i<frames;i++){SteveBridge_Frame(&input,&pose);std::this_thread::sleep_for(std::chrono::microseconds(16667));}};
 auto await=[&](auto predicate,const char* message,int seconds=6){auto until=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);while(!predicate()&&std::chrono::steady_clock::now()<until)run(1);require(predicate(),message);};
 await([&]{return pose.tick>0;},"backend startup",180);input.flags=2;run(200);
 const uint32_t kinds[10]={10,1,3,8,5,13,9,14,15,7};
 for(unsigned i=0;i<10;i++)require(pose.inventory_kind[i]==kinds[i],"exact ten-slot starter loadout");
 require(pose.inventory[5].count==32&&pose.inventory[8].count==1,"obsidian and totem counts");
 for(unsigned i:{2u,4u,7u,9u})require((pose.inventory[i].metadata&0x80000000u)!=0,"unlimited stack metadata");
 require((pose.inventory[6].metadata&0x40000000u)!=0&&pose.equipment_flags==255,"unbreakable elytra and enchanted armor");
 input.slot=9;run(8);require(pose.selected_slot==9&&pose.item_kind==7,"tenth custom slot main hand");
 input.slot=1;run(8);auto durability=pose.inventory[1].durability;auto event=pose.block_event;
 input.buttons=steve::b;await([&]{return pose.block_event>event;},"diamond pickaxe mines stone");input.buttons=0;run(8);require(pose.inventory[1].durability==durability-1,"diamond mining durability");
 input.buttons=steve::r;run(15);require(pose.hand==3&&pose.left_arm<0,"locked shield offhand");input.buttons=0;run(8);
 input.damage=4;run(1);input.damage=0;run(12);require(pose.health<20&&pose.health>16,"enchanted armor protection");float damaged=pose.health;
 input.slot=2;input.buttons=steve::a;run(150);input.buttons=0;run(10);require(pose.inventory[2].count==64&&pose.health>damaged,"unlimited golden apple consumption");
 uint64_t sequence=steve::now_ns();
 auto command=[&](unsigned operation,unsigned a,unsigned b){input.command_sequence=++sequence;input.command=operation;input.argument0=a;input.argument1=b;await([&]{return pose.command_ack==input.command_sequence;},"command acknowledgement");require(pose.command_result==1,"command accepted");input.command_sequence=0;input.command=0;};
 command(1,0,10);run(8);require(pose.inventory_kind[10]==10&&pose.inventory_kind[0]!=10,"inventory swap");command(1,0,10);
 command(2,6,0);run(8);require(!(pose.equipment_flags&2)&&pose.inventory_kind[6]==12,"elytra swaps chestplate");auto elytra=pose.chest.durability;
 input.y=3;input.flags=0;run(8);command(4,0,0);run(30);require(pose.flying&&std::isfinite(pose.flight_z),"vanilla elytra flight");float speed=std::hypot(pose.flight_z,pose.flight_x);
 input.slot=7;input.buttons=steve::a;run(6);input.buttons=0;run(30);require(pose.inventory[7].count==64&&std::hypot(pose.flight_z,pose.flight_x)>speed,"unlimited firework boost");
 run(150);require(pose.chest.durability==elytra,"unbreakable elytra wear");input.flags=2;input.y=0;run(10);require(!pose.flying,"landing ends glide");command(2,6,0);
 input.slot=0;input.target_enemy=1;input.target_token=42;input.target_health=100;input.locked=1;input.tx=0;input.ty=0;input.tz=2;run(80);
 auto attack=pose.attack_event;auto axe=pose.inventory[0].durability;input.buttons=steve::b;run(8);input.buttons=0;run(8);require(pose.attack_event>attack&&pose.attack_damage>0&&pose.inventory[0].durability<axe,"diamond axe combat");
 auto placed=[&](unsigned kind){return std::any_of(std::begin(pose.cells),std::end(pose.cells),[&](const auto& cell){return cell.active&&cell.kind==kind&&cell.flags==0;});};
 input.target_enemy=0;input.flags=10;input.tx=1.5f;input.ty=0.5f;input.tz=1.5f;input.slot=5;input.buttons=steve::a;run(6);input.buttons=0;run(8);require(placed(13)&&pose.inventory[5].count==31,"obsidian placement");
 input.flags=6;input.slot=0;input.buttons=steve::b;run(8);input.buttons=0;run(8);require(!placed(13)&&pose.inventory[5].count==32,"diamond axe obsidian recovery");
 input.slot=4;input.flags=10;input.tx=2.5f;input.buttons=steve::a;run(6);input.buttons=0;run(8);require(placed(5)&&pose.inventory[4].count==64,"unlimited cobblestone placement");
 input.slot=1;input.flags=6;input.buttons=steve::b;await([&]{return !placed(5);},"placed cobblestone mining");input.buttons=0;run(8);
 input.tx=0.5f;input.ty=-0.5f;input.tz=1.5f;event=pose.block_event;input.buttons=steve::b;await([&]{return pose.block_event>event;},"Zelda terrain mining");input.buttons=0;run(8);
 require(std::any_of(std::begin(pose.cells),std::end(pose.cells),[](const auto& cell){return cell.active&&(cell.flags&1);}),"terrain cut published");
 input.flags=2;input.locked=0;input.slot=9;input.buttons=steve::a;run(6);input.buttons=0;run(8);require(pose.tnt_fuse==-1&&pose.inventory[9].count==64,"unlimited TNT placement");
 input.slot=3;auto flint=pose.inventory[3].durability;input.buttons=steve::a;run(6);input.buttons=0;run(8);require(pose.tnt_fuse>0&&pose.inventory[3].durability==flint-1,"flint ignition");await([&]{return pose.tnt_fuse==0;},"TNT detonation",8);
 input.slot=8;run(12);for(int hit=0;hit<12&&pose.inventory[8].count;hit++){input.damage=20;run(1);input.damage=0;run(75);}require(pose.inventory[8].count==0&&pose.health>0,"held Totem of Undying prevents death");
 std::cout<<"PASS Minecraft protocol 4: ten slots, unlimited stacks, shield, golden apples, armor, elytra/fireworks, axe, obsidian recovery, terrain, TNT and totem\n";SteveBridge_Disconnect();
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
