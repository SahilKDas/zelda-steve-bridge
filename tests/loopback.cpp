#include "soh_mod/client.hpp"
#include "headless_mc/logic.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
class Process {
#ifdef _WIN32
    PROCESS_INFORMATION pi{};
#else
    pid_t pid{-1};
#endif
public:
    Process(const std::string& executable,const std::string& session) {
#ifdef _WIN32
        STARTUPINFOA si{}; si.cb=sizeof(si);
        std::string command="\""+executable+"\" "+session+" 10";
        check(CreateProcessA(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0,"process launch failed");
#else
        pid=fork();
        check(pid>=0,"fork failed");
        if(pid==0) { execl(executable.c_str(),executable.c_str(),session.c_str(),"10",nullptr); _exit(127); }
#endif
    }
    ~Process() {
#ifdef _WIN32
        TerminateProcess(pi.hProcess,0); WaitForSingleObject(pi.hProcess,1000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
#else
        kill(pid,SIGTERM); waitpid(pid,nullptr,0);
#endif
    }
    void audit_memory() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX counters{};
        check(GetProcessMemoryInfo(pi.hProcess,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))!=0,"memory query failed");
        check(counters.PeakWorkingSetSize<500'000'000 && counters.PrivateUsage<500'000'000,"backend exceeds RAM budget");
        std::cout<<"backend peak working set bytes="<<counters.PeakWorkingSetSize<<" private bytes="<<counters.PrivateUsage<<'\n';
#endif
    }
};
int main(int argc,char** argv) try {
    check(argc==3,"usage: loopback bridge headless_mc");
    steve::Ring<steve::OotToMcPacket> ring;
    for(unsigned i=0;i<64;++i) { auto* p=ring.reserve(); check(p!=nullptr,"ring reserve"); p->sequence=i; ring.publish(); }
    check(ring.reserve()==nullptr,"full ring must reject writes");
    for(unsigned i=0;i<64;++i) { check(ring.peek()->sequence==i,"FIFO order"); ring.consume(); }
    check(ring.peek()==nullptr,"empty ring");
    ring.write.store(UINT32_MAX-15); ring.read.store(UINT32_MAX-15);
    std::jthread producer([&] { for(unsigned i=0;i<100000;++i) { steve::OotToMcPacket* p; while(!(p=ring.reserve())) std::this_thread::yield(); p->sequence=i; ring.publish(); } });
    for(unsigned i=0;i<100000;++i) { const steve::OotToMcPacket* p; while(!(p=ring.peek())) std::this_thread::yield(); check(p->sequence==i,"concurrent ring ordering"); ring.consume(); }
    producer.join();
    auto invalid=steve::Mapping::open("bad/name",true); check(!invalid,"invalid session must fail");
    steve::World world;
    steve::OotToMcPacket input{}; input.time_ns=steve::now_ns(); input.buttons=steve::a; input.selected_slot=2;
    auto food=world.tick(input); check(food.held.count==7 && food.hand==steve::Hand::consume,"food use");
    check(world.tick(input).held.count==7,"held button must not consume twice");
    input.buttons=0; input.pressed_buttons=steve::a;
    check(world.tick(input).held.count==6,"short button edge must survive tick sampling");
    input.pressed_buttons=0;
    input.time_ns=steve::now_ns()-300'000'000; input.buttons=steve::b; input.selected_slot=0;
    check(world.tick(input).hand==steve::Hand::idle,"stale input must stop actions");
    const std::string session="test_"+std::to_string(steve::now_ns());
    Process bridge(argv[1],session);
    steve::Client client;
    for(int i=0;!client.connect(session) && i<150;++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(client.connect(session),"frontend connection");
    auto collision=steve::Mapping::open(session,true); check(!collision,"duplicate daemon must fail");
    Process backend(argv[2],session);
    input={}; input.buttons=steve::b; input.locked=1; input.target={0,0,2};
    bool mined=false,pose_seen=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(std::chrono::steady_clock::now()<deadline) {
        client.frame(input);
        pose_seen|=client.pose().connected && client.pose().hand==steve::Hand::mine_swing;
        if(client.state().hotbar[4].count>0) { mined=true; break; }
        std::this_thread::sleep_for(std::chrono::microseconds(16667));
    }
    check(mined && pose_seen,"process loopback mining and actor pose");
    check(client.state().held.durability==249,"authoritative tool durability");
    check(client.pop_audio()==steve::Sound::broken_block,"block break audio delivery");
    check(!client.pop_audio(),"audio must not replay each frame");
    input.buttons=steve::r;
    for(int i=0;i<20 && client.state().hand!=steve::Hand::block;++i) { client.frame(input); std::this_thread::sleep_for(std::chrono::microseconds(16667)); }
    check(client.pose().hand==steve::Hand::block,"shield pose");
    input.buttons=0;
    for(int i=0;i<8;++i) { client.frame(input); std::this_thread::sleep_for(std::chrono::microseconds(16667)); }
    input.buttons=steve::a; input.selected_slot=2;
    for(int i=0;i<12;++i) { client.frame(input); std::this_thread::sleep_for(std::chrono::microseconds(16667)); }
    check(client.state().held.id==3 && client.state().held.count==7,"process inventory update");
    check(!client.pose(client.state().time_ns+300'000'000).connected,"stale server state fallback");
    backend.audit_memory();
    std::cout<<"PASS: concurrent wraparound, backpressure, stale inputs, process mining, durability, inventory, actor pose\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
