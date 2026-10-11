#include "bridge/mapping.hpp"
#include "headless_mc/logic.hpp"
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    const std::string session=argc>1?argv[1]:"default";
    const int seconds=argc>2?std::stoi(argv[2]):3600;
    auto mapping=steve::Mapping::open(session,false);
    for(int i=0;!mapping && i<100;++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        mapping=steve::Mapping::open(session,false);
    }
    if(!mapping) { std::cerr<<mapping.error()<<'\n'; return 1; }
    steve::World world;
    steve::OotToMcPacket input{};
    auto& s=mapping->get();
    auto next=std::chrono::steady_clock::now();
    const auto end=next+std::chrono::seconds(seconds);
    while(next<end) {
        std::uint32_t pressed=0;
        while(auto* packet=s.to_backend.peek()) { pressed |= packet->pressed_buttons; input=*packet; s.to_backend.consume(); }
        input.pressed_buttons=pressed;
        auto state=world.tick(input);
        if(auto* packet=s.to_frontend.reserve()) { *packet=state; s.to_frontend.publish(); }
        next+=std::chrono::milliseconds(50);
        std::this_thread::sleep_until(next);
        if(std::chrono::steady_clock::now()-next>std::chrono::milliseconds(50)) next=std::chrono::steady_clock::now();
    }
}
