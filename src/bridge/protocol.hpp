#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace steve {
inline constexpr std::uint32_t magic = 0x53545645, version = 4;
enum Button : std::uint32_t { a=1, b=2, z=4, r=8, c_up=16, c_down=32, c_left=64, c_right=128 };
enum class Hand : std::uint32_t { idle, mine_swing, attack, block, consume, bow };
enum class Sound : std::uint32_t { none, hit, broken_block, eat, shoot };
struct Vec3 { float x{}, y{}, z{}; };
struct alignas(64) OotToMcPacket {
    std::uint64_t sequence{}, time_ns{};
    Vec3 position{};
    float yaw{}, pitch{};
    std::uint32_t buttons{}, locked{}, selected_slot{};
    Vec3 target{};
    std::uint32_t pressed_buttons{};
    float damage{};
    std::uint32_t scene{}, flags{};
    std::uint32_t health_capacity{};
    std::uint64_t command_sequence{};
    std::uint32_t command{}, argument0{}, argument1{}, target_token{};
    float target_health{};
    std::uint32_t target_enemy{};
    std::array<std::uint32_t,4> reserved{};
};
struct Item { std::uint32_t id{}, metadata{}, count{}, durability{}; };
struct TntState { Vec3 position{}; std::int32_t fuse{-2}; std::uint64_t token{}; };
struct WorldCell { Vec3 position{}; std::uint32_t kind{}; std::uint64_t token{}; std::uint32_t active{},flags{}; };
struct alignas(64) McToOotPacket {
    std::uint64_t tick{}, time_ns{}, input_sequence{};
    Item held{};
    Hand hand{};
    float health{20}, swing{};
    std::int32_t destruction_stage{-1};
    std::uint32_t selected_slot{}, audio_count{};
    std::array<Sound, 8> audio{};
    std::array<Item, 9> hotbar{};
    // A completed mining operation acknowledged with a monotonic operation ID.
    std::uint64_t block_event{};
    std::uint32_t block_id{}, backend_kind{};
    std::array<Item,36> inventory{};
    std::array<std::uint32_t,36> inventory_kind{};
    Item chest{};
    std::uint32_t chest_kind{},command_result{};
    std::uint64_t command_ack{},attack_event{};
    float attack_damage{};
    std::uint32_t attack_target{};
    std::uint64_t tnt_event{};
    Vec3 tnt_position{};
    std::int32_t tnt_fuse{-2};
    Vec3 flight_velocity{};
    std::uint32_t flying{};
    float combat_cooldown{};
    std::array<std::uint32_t,5> reserved{};
    std::array<TntState,8> charges{};
    std::array<WorldCell,128> cells{};
};
static_assert(sizeof(OotToMcPacket)==128 && alignof(OotToMcPacket)==64);
static_assert(sizeof(McToOotPacket)==5376 && alignof(McToOotPacket)==64);
static_assert(offsetof(McToOotPacket,cells)==1280 && sizeof(WorldCell)==32);
static_assert(offsetof(OotToMcPacket,command_sequence)==80);
static_assert(offsetof(McToOotPacket,inventory)==256 && offsetof(McToOotPacket,command_ack)==1000);
static_assert(offsetof(McToOotPacket,tnt_event)==1024 && offsetof(McToOotPacket,flying)==1060);
static_assert(offsetof(McToOotPacket,charges)==1088 && sizeof(TntState)==24);
static_assert(offsetof(OotToMcPacket, target)==48);
static_assert(offsetof(OotToMcPacket, damage)==64 && offsetof(OotToMcPacket, health_capacity)==76);
static_assert(offsetof(McToOotPacket, hotbar)==96 && offsetof(McToOotPacket, block_event)==240);
static_assert(std::is_trivially_copyable_v<McToOotPacket>);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
// One producer and one consumer per ring. The slot remains owned until consume().
// Same architecture/compiler ABI is required; this is not a network wire format.
template<class T, std::uint32_t N=64> struct alignas(64) Ring {
    alignas(64) std::atomic<std::uint32_t> write{0};
    std::array<std::byte,60> write_padding{};
    alignas(64) std::atomic<std::uint32_t> read{0};
    std::array<std::byte,60> read_padding{};
    alignas(64) std::array<T,N> slots{};
    T* reserve() noexcept {
        auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=N) return nullptr;
        return &slots[w%N];
    }
    void publish() noexcept { write.fetch_add(1,std::memory_order_release); }
    const T* peek() noexcept {
        auto q=read.load(std::memory_order_relaxed);
        return q==write.load(std::memory_order_acquire)?nullptr:&slots[q%N];
    }
    void consume() noexcept { read.fetch_add(1,std::memory_order_release); }
};
struct alignas(64) Shared {
    std::atomic<std::uint32_t> ready{0};
    std::uint32_t protocol{version}, bytes{sizeof(Shared)};
    std::array<std::byte,52> header_padding{};
    alignas(64) std::atomic<std::uint64_t> heartbeat_ns{0};
    std::array<std::byte,56> heartbeat_padding{};
    Ring<OotToMcPacket> to_backend;
    Ring<McToOotPacket> to_frontend;
};
static_assert(offsetof(Shared, heartbeat_ns)==64 && offsetof(Shared, to_backend)==128);
static_assert(offsetof(Ring<OotToMcPacket>, slots)==128);
inline std::uint64_t now_ns() noexcept;
}
#include <chrono>
namespace steve {
inline std::uint64_t now_ns() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}
