/* In-tree SoH adapter. This file needs SoH's global.h and libultraship. */
#include "global.h"
#include "soh_mod/soh_api.h"
#include <libultraship/bridge/consolevariablebridge.h>

static StevePose sPose;
static int sConnected;
static uint32_t sSlot;

void SteveActor_Update(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    Input* input = &play->state.input[0];
    SteveInput packet = {0};
    if (!CVarGetInteger("gEnhancements.SteveBridge", 0)) { sConnected = 0; return; }
    if (input->press.button & BTN_CRIGHT) sSlot = (sSlot + 1) % 9;
    if (input->press.button & BTN_CLEFT) sSlot = (sSlot + 8) % 9;
    packet.x = actor->world.pos.x / 20.0f;
    packet.y = actor->world.pos.y / 20.0f;
    packet.z = actor->world.pos.z / 20.0f;
    packet.yaw = actor->shape.rot.y * (3.14159265f / 32768.0f);
    packet.pitch = actor->focus.rot.x * (3.14159265f / 32768.0f);
    packet.slot = sSlot;
    if (!(player->stateFlags1 & (PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_IN_CUTSCENE))) {
        if (input->cur.button & BTN_A) packet.buttons |= 1;
        if (input->cur.button & BTN_B) packet.buttons |= 2;
        if (input->cur.button & BTN_Z) packet.buttons |= 4;
        if (input->cur.button & BTN_R) packet.buttons |= 8;
        if (input->cur.button & BTN_CUP) packet.buttons |= 16;
        if (input->cur.button & BTN_CDOWN) packet.buttons |= 32;
        if (input->cur.button & BTN_CLEFT) packet.buttons |= 64;
        if (input->cur.button & BTN_CRIGHT) packet.buttons |= 128;
    }
    packet.locked = player->focusActor != NULL;
    if (player->focusActor != NULL) {
        packet.tx = player->focusActor->world.pos.x / 20.0f;
        packet.ty = player->focusActor->world.pos.y / 20.0f;
        packet.tz = player->focusActor->world.pos.z / 20.0f;
    }
    sConnected = SteveBridge_Frame(&packet, &sPose);
}

/* Original, untextured cube mesh: no copyrighted model assets required. */
static Vtx sCube[] = {
    {{{-1,-1,-1},0,{0,0},{255,255,255,255}}},
    {{{ 1,-1,-1},0,{0,0},{255,255,255,255}}},
    {{{ 1, 1,-1},0,{0,0},{255,255,255,255}}},
    {{{-1, 1,-1},0,{0,0},{255,255,255,255}}},
    {{{-1,-1, 1},0,{0,0},{255,255,255,255}}},
    {{{ 1,-1, 1},0,{0,0},{255,255,255,255}}},
    {{{ 1, 1, 1},0,{0,0},{255,255,255,255}}},
    {{{-1, 1, 1},0,{0,0},{255,255,255,255}}},
};
static Gfx sCubeDL[] = {
    gsSPVertex(sCube,8,0),
    gsSP2Triangles(0,2,1,0,0,3,2,0), gsSP2Triangles(4,5,6,0,4,6,7,0),
    gsSP2Triangles(0,1,5,0,0,5,4,0), gsSP2Triangles(3,7,6,0,3,6,2,0),
    gsSP2Triangles(0,4,7,0,0,7,3,0), gsSP2Triangles(1,2,6,0,1,6,5,0),
    gsSPEndDisplayList(),
};
static void SteveBox(PlayState* play,float x,float y,float z,float w,float h,float d,float rx,
                     int red,int green,int blue) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(x,y,z,MTXMODE_APPLY);
    Matrix_RotateX(rx,MTXMODE_APPLY);
    Matrix_Scale(w,h,d,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetPrimColor(POLY_OPA_DISP++,0,0,red,green,blue,255);
    gSPDisplayList(POLY_OPA_DISP++,sCubeDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
int SteveActor_Draw(Actor* actor, PlayState* play) {
    if (!sConnected || !CVarGetInteger("gEnhancements.SteveBridge",0)) return 0;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768.0f),MTXMODE_APPLY);
    SteveBox(play,0,31,0,6,6,6,0,165,117,82);
    SteveBox(play,0,20,0,6,6,3,0,0,170,180);
    SteveBox(play,-8,20,0,2,6,3,sPose.right_arm,165,117,82);
    SteveBox(play,8,20,0,2,6,3,sPose.left_arm,165,117,82);
    SteveBox(play,-3,7,0,3,7,3,0,65,65,150);
    SteveBox(play,3,7,0,3,7,3,0,65,65,150);
    if (sPose.item != 0) SteveBox(play,-8,14,4,1,5,1,sPose.right_arm,120,90,50);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
    return 1;
}
