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
} SteveInput;
typedef struct SteveItem { uint32_t id,metadata,count,durability; } SteveItem;
typedef struct StevePose {
    float right_arm,left_arm;
    uint32_t item,hand;
    float health;
    int32_t destruction_stage;
    uint64_t tick,block_event;
    uint32_t item_kind,selected_slot,block_id,reserved;
    SteveItem hotbar[9];
} StevePose;
int SteveBridge_Frame(const SteveInput* input, StevePose* pose);
int SteveBridge_GetPose(StevePose* pose);
int SteveBridge_PopAudio(void);
void SteveBridge_Disconnect(void);
#ifdef __cplusplus
}
#endif
