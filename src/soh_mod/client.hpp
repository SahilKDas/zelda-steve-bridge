#pragma once
#include "bridge/mapping.hpp"
#include <optional>
namespace steve {
struct Pose { Hand hand{}; float swing{}, right_arm{}, left_arm{}; bool connected{}; };
class Client {
    std::optional<Mapping> mapping;
    McToOotPacket previous{}, current{};
    std::uint64_t sequence{}, retry_at{};
    std::uint32_t last_buttons{}, pending_pressed{}, audio_read{}, audio_write{}, dropped_audio{};
    std::array<Sound,64> sounds{};
public:
    bool connect(const std::string& session);
    void disconnect();
    bool frame(OotToMcPacket input);
    Pose pose(std::uint64_t time=now_ns()) const;
    const McToOotPacket& state() const { return current; }
    std::optional<Sound> pop_audio();
    std::uint32_t audio_drops() const { return dropped_audio; }
};
}
