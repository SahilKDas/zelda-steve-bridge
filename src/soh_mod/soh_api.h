#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SteveInput {
    float x,y,z,yaw,pitch,tx,ty,tz;
    uint32_t buttons,locked,slot;
} SteveInput;
typedef struct StevePose {
    float right_arm,left_arm;
    uint32_t item,hand;
    float health;
    int32_t destruction_stage;
} StevePose;
int SteveBridge_Frame(const SteveInput* input, StevePose* pose);
#ifdef __cplusplus
}
#endif
