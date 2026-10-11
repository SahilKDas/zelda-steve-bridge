#pragma once
#include "bridge/protocol.hpp"
#include <expected>
#include <memory>
#include <string>
namespace steve {
class Mapping {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit Mapping(std::unique_ptr<Impl> p);
public:
    ~Mapping();
    Mapping(Mapping&&) noexcept;
    Mapping& operator=(Mapping&&) noexcept;
    Mapping(const Mapping&)=delete;
    static std::expected<Mapping,std::string> open(const std::string& name, bool create);
    Shared& get() noexcept;
};
}
