#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SteveInput {
    float x,y,z,yaw,pitch,tx,ty,tz;
    uint32_t buttons,locked,slot;
    float damage;
    uint32_t scene,flags,health_capacity;
    uint32_t reserved;
    uint64_t command_sequence;
    uint32_t command,argument0,argument1,target_token;
    float target_health;
    uint32_t target_enemy;
} SteveInput;
typedef struct SteveItem { uint32_t id,metadata,count,durability; } SteveItem;
typedef struct SteveTnt { float x,y,z; int32_t fuse; uint64_t token; } SteveTnt;
typedef struct SteveCell { float x,y,z; uint32_t kind; uint64_t token; uint32_t active,flags; } SteveCell;
typedef struct StevePose {
    float right_arm,left_arm;
    uint32_t item,hand;
    float health;
    int32_t destruction_stage;
    uint64_t tick,block_event;
    uint32_t item_kind,selected_slot,block_id,equipment_flags;
    SteveItem hotbar[9];
    SteveItem inventory[36];
    uint32_t inventory_kind[36];
    SteveItem chest;
    uint64_t command_ack,attack_event,tnt_event;
    float attack_damage,tnt_x,tnt_y,tnt_z,flight_x,flight_y,flight_z,combat_cooldown;
    uint32_t attack_target,flying,command_result;
    int32_t tnt_fuse;
    SteveTnt charges[8];
    SteveCell cells[128];
} StevePose;
int SteveBridge_Frame(const SteveInput* input, StevePose* pose);
int SteveBridge_GetPose(StevePose* pose);
int SteveBridge_PopAudio(void);
void SteveBridge_Disconnect(void);
int SteveBridge_RequestCommand(uint32_t command,uint32_t argument0,uint32_t argument1);
#ifdef __cplusplus
}
#endif
