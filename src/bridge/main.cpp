#include "bridge/mapping.hpp"
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    const std::string session=argc>1?argv[1]:"default";
    const int seconds=argc>2?std::stoi(argv[2]):3600;
    auto mapping=steve::Mapping::open(session,true);
    if(!mapping) { std::cerr<<mapping.error()<<'\n'; return 1; }
    auto& s=mapping->get();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(std::chrono::steady_clock::now()<deadline) {
        s.heartbeat_ns.store(steve::now_ns(),std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    s.ready.store(0,std::memory_order_release);
}
