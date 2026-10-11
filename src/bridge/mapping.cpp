#include "bridge/mapping.hpp"
#include <new>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace steve {
struct Mapping::Impl {
    Shared* data{};
    std::string name;
    bool owner{};
#ifdef _WIN32
    HANDLE handle{};
#else
    int fd{-1};
#endif
    ~Impl() {
#ifdef _WIN32
        if(data) UnmapViewOfFile(data);
        if(handle) CloseHandle(handle);
#else
        if(data) munmap(data,sizeof(Shared));
        if(fd>=0) close(fd);
        if(owner) shm_unlink(name.c_str());
#endif
    }
};
Mapping::Mapping(std::unique_ptr<Impl> p):impl(std::move(p)){}
Mapping::~Mapping()=default;
Mapping::Mapping(Mapping&&) noexcept=default;
Mapping& Mapping::operator=(Mapping&&) noexcept=default;
Shared& Mapping::get() noexcept { return *impl->data; }
std::expected<Mapping,std::string> Mapping::open(const std::string& name,bool create) {
    if(name.empty() || name.size()>100 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
        return std::unexpected("invalid session name");
    auto p=std::make_unique<Impl>();
#ifdef _WIN32
    p->name="Local\\zelda_steve_"+name;
    if(create) {
        p->handle=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),p->name.c_str());
        if(p->handle && GetLastError()==ERROR_ALREADY_EXISTS) return std::unexpected("session already exists");
    } else p->handle=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,p->name.c_str());
    if(!p->handle) return std::unexpected("cannot open shared memory");
    p->data=static_cast<Shared*>(MapViewOfFile(p->handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
    if(!p->data) return std::unexpected("cannot map shared memory");
#else
    p->name="/zelda_steve_"+name;
    p->fd=shm_open(p->name.c_str(),O_RDWR|(create?(O_CREAT|O_EXCL):0),0600);
    if(p->fd<0) return std::unexpected("cannot open shared memory");
    p->owner=create;
    if(create && ftruncate(p->fd,sizeof(Shared))!=0) return std::unexpected("cannot size shared memory");
    struct stat st{};
    if(fstat(p->fd,&st)!=0 || st.st_size!=sizeof(Shared)) return std::unexpected("shared memory size mismatch");
    auto ptr=mmap(nullptr,sizeof(Shared),PROT_READ|PROT_WRITE,MAP_SHARED,p->fd,0);
    if(ptr==MAP_FAILED) return std::unexpected("cannot map shared memory");
    p->data=static_cast<Shared*>(ptr);
#endif
    if(create) {
        std::construct_at(p->data);
        p->data->ready.store(magic,std::memory_order_release);
    } else if(p->data->ready.load(std::memory_order_acquire)!=magic || p->data->protocol!=version || p->data->bytes!=sizeof(Shared))
        return std::unexpected("session not ready or ABI mismatch");
    return Mapping(std::move(p));
}
}
