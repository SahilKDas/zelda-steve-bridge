/* In-tree SoH adapter. This file needs SoH's global.h and libultraship. */
#include "global.h"
#include "soh_mod/soh_api.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <string.h>
#include <math.h>

static StevePose sPose;
static int sConnected;
static uint32_t sSlot;
int SteveActor_IsReady(void) { return sConnected && CVarGetInteger("gEnhancements.SteveBridge",1); }
static PlayState* sPlay;
static Vec3f sOrigin;
static uint64_t sLastTick, sBlockEvent;
static int sLastHealth, sBlockAlive;
static DynaPolyActor sBlock;
static Vec3s sBlockVertices[] = {
    {-10,-10,-10},{10,-10,-10},{10,10,-10},{-10,10,-10},
    {-10,-10,10},{10,-10,10},{10,10,10},{-10,10,10}
};
static CollisionPoly sBlockPolys[12];
static SurfaceType sBlockSurface[] = {{{0,0}}};
static CollisionHeader sBlockCollision = {
    {-10,-10,-10},{10,10,10},8,sBlockVertices,12,sBlockPolys,sBlockSurface,NULL,0,NULL,0
};
static void SteveBlock_NoUpdate(Actor* actor, PlayState* play) { (void)actor; (void)play; }
void SteveActor_Reset(PlayState* play) {
    if (sPlay == play && sBlockAlive && sBlock.bgId < BG_ACTOR_MAX)
        DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sBlock.bgId);
    sPlay = NULL; sBlockAlive = 0; sConnected = 0; sLastTick = 0;
    SteveBridge_Disconnect();
}
static void SteveBlock_Create(PlayState* play) {
    static const u16 triangles[12][3] = {
        {0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
        {3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}
    };
    static const Vec3s normals[6] = {{0,0,-32767},{0,0,32767},{0,-32767,0},{0,32767,0},{-32767,0,0},{32767,0,0}};
    int i;
    memset(&sBlock,0,sizeof(sBlock));
    for(i=0;i<12;i++) {
        sBlockPolys[i].type=0;
        memcpy(sBlockPolys[i].vtxData,triangles[i],sizeof(triangles[i]));
        sBlockPolys[i].normal=normals[i/2]; sBlockPolys[i].dist=-10;
    }
    sBlock.actor.id=ACTOR_BG_HEAVY_BLOCK;
    sBlock.actor.update=SteveBlock_NoUpdate;
    sBlock.actor.world.pos=sOrigin;
    sBlock.actor.world.pos.y+=10; sBlock.actor.world.pos.z+=40;
    sBlock.actor.scale.x=sBlock.actor.scale.y=sBlock.actor.scale.z=1;
    DynaPolyActor_Init(&sBlock,DPM_UNK);
    sBlock.bgId=DynaPoly_SetBgActor(play,&play->colCtx.dyna,&sBlock.actor,&sBlockCollision);
    sBlockAlive=1;
}

void SteveActor_Update(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    Input* input = &play->state.input[0];
    SteveInput packet = {0};
    if (!CVarGetInteger("gEnhancements.SteveBridge", 1)) { if (sPlay) SteveActor_Reset(sPlay); return; }
    if (sPlay != play) {
        sPlay=play; sOrigin=actor->world.pos; sLastHealth=gSaveContext.health;
        sLastTick=0; sBlockEvent=0; sBlockAlive=0;
    }
    if (input->press.button & BTN_CRIGHT) sSlot = (sSlot + 1) % 9;
    if (input->press.button & BTN_CLEFT) sSlot = (sSlot + 8) % 9;
    packet.x = actor->world.pos.x / 20.0f;
    packet.y = actor->world.pos.y / 20.0f;
    packet.z = actor->world.pos.z / 20.0f;
    packet.yaw = actor->shape.rot.y * (3.14159265f / 32768.0f);
    packet.pitch = actor->focus.rot.x * (3.14159265f / 32768.0f);
    packet.slot = sSlot;
    packet.scene=(uint32_t)play->sceneNum;
    packet.health_capacity=(uint32_t)gSaveContext.healthCapacity;
    if (sConnected && gSaveContext.health < sLastHealth && gSaveContext.healthCapacity > 0)
        packet.damage=(sLastHealth-gSaveContext.health)*20.0f/gSaveContext.healthCapacity;
    if (sConnected && sPose.health <= 0 && gSaveContext.health > 0) packet.flags=1;
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
    /* The arena cube is a native dynamic collision object and the default mining target. */
    if (!packet.locked) {
        packet.tx=sOrigin.x/20.0f;
        packet.ty=(sOrigin.y+10)/20.0f;
        packet.tz=(sOrigin.z+40)/20.0f;
    }
    sConnected = SteveBridge_Frame(&packet, &sPose);
    if (sConnected && sPose.tick != sLastTick) {
        int sound;
        if (!sLastTick) { SteveBlock_Create(play); sBlockEvent=sPose.block_event; }
        sLastTick=sPose.tick;
        if (sPose.block_event != sBlockEvent) {
            sBlockEvent=sPose.block_event;
            if (sBlockAlive && sBlock.bgId < BG_ACTOR_MAX) DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sBlock.bgId);
            sBlockAlive=0;
        }
        if (gSaveContext.healthCapacity > 0) {
            int health=(int)lroundf(sPose.health*gSaveContext.healthCapacity/20.0f);
            if (health < 0) health=0;
            if (health > gSaveContext.healthCapacity) health=gSaveContext.healthCapacity;
            gSaveContext.health=(s16)health;
        }
        while ((sound=SteveBridge_PopAudio()) != 0) {
            if (sound==1) Sfx_PlaySfxCentered(NA_SE_IT_SWORD_SWING);
            else if (sound==2) Sfx_PlaySfxCentered(NA_SE_EV_ROCK_BROKEN);
            else if (sound==3) Sfx_PlaySfxCentered(NA_SE_SY_GET_ITEM);
            else if (sound==4) Sfx_PlaySfxCentered(NA_SE_IT_ARROW_SHOT);
        }
    }
    sLastHealth=gSaveContext.health;
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
    if (!sConnected || !CVarGetInteger("gEnhancements.SteveBridge",1)) return 0;
    if (!SteveBridge_GetPose(&sPose)) return 0;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768.0f),MTXMODE_APPLY);
    SteveBox(play,0,31,0,6,6,6,0,165,117,82);
    SteveBox(play,0,36,0,6.1f,1.1f,6.1f,0,65,40,25);
    SteveBox(play,-2.8f,32,6.1f,1.3f,0.8f,0.2f,0,235,235,235);
    SteveBox(play,2.8f,32,6.1f,1.3f,0.8f,0.2f,0,235,235,235);
    SteveBox(play,-2.3f,32,6.4f,0.5f,0.8f,0.2f,0,65,45,150);
    SteveBox(play,2.3f,32,6.4f,0.5f,0.8f,0.2f,0,65,45,150);
    SteveBox(play,0,28,6.1f,2.5f,1,0.3f,0,85,50,35);
    SteveBox(play,0,20,0,6,6,3,0,0,170,180);
    {
        float gait=sinf(play->gameplayFrames*0.3f)*fminf(fabsf(actor->speedXZ)*0.16f,0.8f);
        Matrix_Push();
        Matrix_Translate(-8,25,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.right_arm+gait,MTXMODE_APPLY);
        SteveBox(play,0,-5,0,2,6,3,0,165,117,82);
        SteveBox(play,0,0,0,2.1f,2,3.1f,0,0,170,180);
        if (sPose.item_kind==1) {
            SteveBox(play,0,-11,4,0.8f,5,0.8f,0,120,90,50);
            SteveBox(play,0,-7,4,4,1,1,0,190,190,195);
        } else if (sPose.item_kind==2) {
            SteveBox(play,0,-10,4,0.8f,2,0.8f,0,100,65,35);
            SteveBox(play,0,-8,4,3,0.7f,1,0,165,165,170);
            SteveBox(play,0,-4,4,1,4,0.5f,0,210,210,220);
        } else if (sPose.item_kind==3) SteveBox(play,0,-10,4,2,1.5f,2,0,120,55,25);
        else if (sPose.item_kind==4) {
            SteveBox(play,0,-10,4,0.5f,5,0.5f,0,125,90,45);
            SteveBox(play,0,-5,3,0.5f,0.5f,1.5f,0,125,90,45);
            SteveBox(play,0,-15,3,0.5f,0.5f,1.5f,0,125,90,45);
            SteveBox(play,0,-10,2,0.2f,5,0.2f,0,230,220,190);
        } else if (sPose.item_kind==5) SteveBox(play,0,-10,4,2,2,2,0,120,120,120);
        Matrix_Pop();
        Matrix_Push();
        Matrix_Translate(8,25,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.left_arm-gait,MTXMODE_APPLY);
        SteveBox(play,0,-5,0,2,6,3,0,165,117,82);
        SteveBox(play,0,0,0,2.1f,2,3.1f,0,0,170,180);
        if (sPose.hand==3) SteveBox(play,0,-6,4,4,5,0.8f,0,125,85,45);
        Matrix_Pop();
        SteveBox(play,-3,7,0,3,7,3,-gait,65,65,150);
        SteveBox(play,3,7,0,3,7,3,gait,65,65,150);
    }
    Matrix_Pop();
    if (sBlockAlive) {
        int shade=145-sPose.destruction_stage*7;
        Matrix_Push();
        Matrix_Translate(sBlock.actor.world.pos.x,sBlock.actor.world.pos.y,sBlock.actor.world.pos.z,MTXMODE_NEW);
        SteveBox(play,0,0,0,10,10,10,0,shade,shade,shade);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
    {
        GfxPrint printer;
        int i;
        OPEN_DISPS(play->state.gfxCtx);
        GfxPrint_Init(&printer);
        GfxPrint_Open(&printer,OVERLAY_DISP);
        GfxPrint_SetColor(&printer,255,255,255,255);
        GfxPrint_SetPos(&printer,2,25);
        GfxPrint_Printf(&printer,"MC ");
        for(i=0;i<9;i++) {
            if ((uint32_t)i==sPose.selected_slot) GfxPrint_SetColor(&printer,255,215,60,255);
            else GfxPrint_SetColor(&printer,190,190,190,255);
            GfxPrint_Printf(&printer,"%d:%d ",i+1,(int)sPose.hotbar[i].count);
        }
        GfxPrint_SetColor(&printer,255,255,255,255);
        GfxPrint_SetPos(&printer,2,26);
        GfxPrint_Printf(&printer,"Tool %u  Mining %d",sPose.hotbar[sPose.selected_slot%9].durability,sPose.destruction_stage);
        OVERLAY_DISP=GfxPrint_Close(&printer);
        GfxPrint_Destroy(&printer);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return 1;
}
