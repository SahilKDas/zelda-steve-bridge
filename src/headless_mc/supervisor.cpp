#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
std::string quote(const std::string& value) {
    std::string result="\"";
    unsigned slashes=0;
    for(char c:value) {
        if(c=='\\') { ++slashes; continue; }
        if(c=='\"') result.append(slashes*2+1,'\\');
        else result.append(slashes,'\\');
        slashes=0; result+=c;
    }
    result.append(slashes*2,'\\'); result+='\"'; return result;
}
std::uintmax_t bytes(const fs::path& path) {
    std::uintmax_t total=0;
    for(const auto& entry:fs::recursive_directory_iterator(path)) if(entry.is_regular_file()) total+=entry.file_size();
    return total;
}
int main(int argc,char** argv) try {
    if(argc<4) { std::cerr<<"usage: mc_supervisor JAVA RUNTIME JNI [SECONDS] [SESSION]\n"; return 1; }
    auto java=fs::absolute(argv[1]).string(), runtime=fs::absolute(argv[2]).string(), jni=fs::absolute(argv[3]).string();
    const int seconds=argc>4?std::stoi(argv[4]):3600;
    const std::string session=argc>5?argv[5]:"default";
    if(!fs::exists(fs::path(runtime)/"eula.txt")) throw std::runtime_error("Minecraft EULA acceptance file is missing");
    if(bytes(runtime)>=200'000'000) throw std::runtime_error("runtime exceeds 200 MB");
    std::vector<std::string> arguments={java,"-Xms64m","-Xmx256m","-XX:+UseSerialGC","-XX:MaxMetaspaceSize=112m",
        "-XX:CompressedClassSpaceSize=24m","-XX:ReservedCodeCacheSize=16m","-XX:TieredStopAtLevel=1","-XX:ActiveProcessorCount=2","-Xss512k","-XX:MaxDirectMemorySize=8m",
        "-Djava.awt.headless=true","-Dio.netty.eventLoopThreads=2","-Dsteve.native="+jni,"-Dsteve.session="+session,
        "-jar","fabric-server-launch.jar","nogui"};
    std::string command;
    for(const auto& arg:arguments) { if(!command.empty()) command+=' '; command+=quote(arg); }
    HANDLE job=CreateJobObjectA(nullptr,nullptr);
    if(!job) throw std::runtime_error("CreateJobObject failed");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit=480'000'000;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) { CloseHandle(job); throw std::runtime_error("memory limit setup failed"); }
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE log=CreateFileA((fs::path(runtime)/"server-console.log").string().c_str(),GENERIC_WRITE,FILE_SHARE_READ,
                          &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE input=CreateFileA("NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOA si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=log; si.hStdError=log; si.hStdInput=input;
    PROCESS_INFORMATION pi{};
    if(!CreateProcessA(java.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,runtime.c_str(),&si,&pi)) {
        CloseHandle(log); CloseHandle(input); CloseHandle(job); throw std::runtime_error("Java process launch failed");
    }
    CloseHandle(log); CloseHandle(input);
    if(!AssignProcessToJobObject(job,pi.hProcess)) { TerminateProcess(pi.hProcess,1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job); throw std::runtime_error("Java job assignment failed"); }
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    SIZE_T peakWorking=0,peakPrivate=0;
    bool violation=false;
    for(int elapsed=0;elapsed<seconds;++elapsed) {
        if(WaitForSingleObject(pi.hProcess,1000)==WAIT_OBJECT_0) break;
        PROCESS_MEMORY_COUNTERS_EX memory{};
        if(GetProcessMemoryInfo(pi.hProcess,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))) {
            peakWorking=std::max(peakWorking,memory.PeakWorkingSetSize); peakPrivate=std::max(peakPrivate,memory.PrivateUsage);
        }
        if(peakWorking>=500'000'000 || bytes(runtime)>=200'000'000) {
            violation=true; std::cerr<<"Minecraft runtime resource ceiling exceeded\n"; break;
        }
    }
    DWORD code=0;
    GetExitCodeProcess(pi.hProcess,&code);
    if(code==STILL_ACTIVE) { TerminateJobObject(job,violation?1:0); WaitForSingleObject(pi.hProcess,2000); code=violation?1:0; }
    CloseHandle(pi.hProcess); CloseHandle(job);
    std::cout<<"Minecraft peak working bytes="<<peakWorking<<" peak private bytes="<<peakPrivate<<" runtime bytes="<<bytes(runtime)<<'\n';
    return code==0 && !violation?0:1;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
