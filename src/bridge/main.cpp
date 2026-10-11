#include "bridge/mapping.hpp"
#include <iostream>
#include <thread>
template<class T> void relay(steve::Ring<T>& from,steve::Ring<T>& to) {
    while(auto* input=from.peek()) {
        auto* output=to.reserve();
        if(!output) break;
        *output=*input;
        to.publish();
        from.consume();
    }
}
int main(int argc,char** argv) {
    const std::string session=argc>1?argv[1]:"default";
    const int seconds=argc>2?std::stoi(argv[2]):3600;
    auto mapping=steve::Mapping::open(session,true);
    if(!mapping) { std::cerr<<mapping.error()<<'\n'; return 1; }
    auto& s=mapping->get();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(std::chrono::steady_clock::now()<deadline) {
        relay(s.frontend_in,s.backend_in);
        relay(s.backend_out,s.frontend_out);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
