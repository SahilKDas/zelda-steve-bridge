/* In-tree SoH adapter. This file needs SoH's global.h and libultraship. */
#include "global.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "soh_mod/soh_api.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "soh_mod/generated/minecraft_textures.inc"

static StevePose sPose;
static int sConnected;
static uint32_t sSlot;
static uint32_t sSceneSerial;
static int sAnchorFrames;
int SteveUi_IsOpen(void);
void SteveActor_RequestExit(void);
void SteveUi_TestInventory(int open);
int SteveUi_TestDraws(void);
int SteveUi_TextureCount(void);
int SteveActor_IsReady(void) { return sConnected && CVarGetInteger("gEnhancements.SteveBridge",1) && SteveBridge_GetPose(&sPose); }
static PlayState* sPlay;
static Vec3f sOrigin;
static uint64_t sLastTick, sBlockEvent;
static int sLastHealth, sBlockAlive;
static DynaPolyActor sBlock;
static DynaPolyActor sTnt[8];
static int sTntCollision[8];
static uint64_t sChargeToken[8];
static int sChargeFuse[8];
static int sTntAlive;
static uint64_t sTntEvent,sAttackEvent;
static ColliderQuad sAxe;
static int sAxeInitialized;
static int sWasFlying;
static float sGravityBeforeFlight;
static int sNativeAxeHits;
static ColliderQuadInit sAxeInit = {
    {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_QUAD},
    {ELEMTYPE_UNK0,{DMG_HAMMER_SWING,0,1},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_NONE},
    {{{0,0,0},{0,0,0},{0,0,0},{0,0,0}}}
};
int SteveActor_IsAttackInfo(const ColliderInfo* info) {
    int matched=sAxeInitialized && info==&sAxe.info;
    if(matched) sNativeAxeHits++;
    return matched;
}
static int sTestMode=-1, sTestFrame, sRenderedFrames;
static uint32_t sTestPick, sTestFood, sTestBow, sTestArrows;
static int sTestShield, sTestDamage, sTestCollision;
static int sTestBasePassed,sTestTntPlaced,sTestTntPrimed,sTestTntExplosion,sTestSwap,sTestGlide;
static uint32_t sTestEnemyToken;
static int sTestEnemyHealth,sTestEnemyDamaged;
static Vec3f sTestFlightStart;
static void SteveActor_TestInput(SteveInput* packet) {
    if (sTestMode < 0) sTestMode=getenv("STEVE_IN_GAME_TEST") != NULL;
    if (!sTestMode) return;
    packet->buttons=0; packet->locked=0; packet->slot=0;
    if (!sConnected) { packet->flags|=1; return; }
    sTestFrame++;
    if (sTestFrame==175) sTestShield=sPose.hand==3;
    if (sTestFrame==210) sTestDamage=sPose.health<=16.1f;
    if (sTestFrame==1) {
        packet->flags|=1;
        sTestPick=sPose.hotbar[0].durability; sTestFood=sPose.hotbar[2].count;
        sTestBow=sPose.hotbar[3].durability; sTestArrows=sPose.hotbar[8].count;
    }
    if (sTestFrame>=100 && sTestFrame<160) packet->buttons=2;
    else if (sTestFrame>=160 && sTestFrame<180) packet->buttons=8;
    else if (sTestFrame==190) packet->damage=4;
    else if (sTestFrame>=220 && sTestFrame<300) {packet->slot=2;packet->buttons=1;}
    else if (sTestFrame>=310 && sTestFrame<350) {packet->slot=3;packet->buttons=1;}
    else if (sTestFrame>=350) packet->slot=3;
    if (sTestFrame==380) {
        FILE* log=fopen("steve-in-game-test.log","w");
        int passed=sRenderedFrames>20 && sTestCollision && !sBlockAlive && sTestShield && sTestDamage &&
            sPose.hotbar[0].durability==sTestPick-1 && sPose.hotbar[2].count==sTestFood-1 &&
            sPose.hotbar[3].durability==sTestBow-1 && sPose.hotbar[8].count==sTestArrows-1;
        if (log) {
            fprintf(log,"%s native SoH actor: rendered=%d tick=%llu mining_ack=%llu block_alive=%d pickaxe=%u food=%u bow=%u arrows=%u health=%.2f\n",
                passed?"PASS":"FAIL",sRenderedFrames,(unsigned long long)sPose.tick,
                (unsigned long long)sPose.block_event,sBlockAlive,sPose.hotbar[0].durability,
                sPose.hotbar[2].count,sPose.hotbar[3].durability,sPose.hotbar[8].count,sPose.health);
            fclose(log);
        }
        sTestBasePassed=passed;
    }
    if(sTestFrame>=390 && sTestFrame<395) {packet->slot=5;packet->buttons=1;}
    if(sTestFrame==410) sTestTntPlaced=sTntAlive && sPose.hotbar[5].count==15;
    if(sTestFrame>=415 && sTestFrame<420) {packet->slot=6;packet->buttons=1;}
    if(sTestFrame==435) sTestTntPrimed=sPose.tnt_fuse>0 && sPose.hotbar[6].durability==63;
    if(sTestFrame==520) {
        Actor* enemy=Actor_Spawn(&sPlay->actorCtx,sPlay,ACTOR_EN_IK,
            GET_PLAYER(sPlay)->actor.world.pos.x,GET_PLAYER(sPlay)->actor.world.pos.y,
            GET_PLAYER(sPlay)->actor.world.pos.z+30,0,0,0,1);
        if(enemy) {enemy->colChkInfo.health=40;sTestEnemyToken=(uint32_t)(((uintptr_t)enemy>>4)^sSceneSerial);}
    }
    if(sTestFrame==525 || sTestFrame==670) {
        Actor* enemy=sPlay->actorCtx.actorLists[ACTORCAT_ENEMY].head;
        for(;enemy;enemy=enemy->next)if((uint32_t)(((uintptr_t)enemy>>4)^sSceneSerial)==sTestEnemyToken)break;
        if(sTestFrame==525 && enemy)sTestEnemyHealth=enemy->colChkInfo.health;
        if(sTestFrame==670)sTestEnemyDamaged=sTestEnemyHealth>0 && (!enemy || enemy->colChkInfo.health<sTestEnemyHealth);
    }
    if(sTestFrame>=525 && sTestFrame<650) {packet->slot=4;if(sTestFrame%20<5)packet->buttons=2;}
    if(sTestFrame==680) SteveBridge_RequestCommand(1,4,10);
    if(sTestFrame==710) {sTestSwap=sPose.inventory_kind[10]==10;SteveUi_TestInventory(1);}
    if(sTestFrame==735) {SteveUi_TestInventory(0);SteveBridge_RequestCommand(1,4,10);}
    if(sTestFrame==760) SteveBridge_RequestCommand(2,7,0);
    if(sTestFrame==780) {
        GET_PLAYER(sPlay)->actor.world.pos.y+=120;
        sTestFlightStart=GET_PLAYER(sPlay)->actor.world.pos;
        GET_PLAYER(sPlay)->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;
        packet->y=GET_PLAYER(sPlay)->actor.world.pos.y/20;packet->flags&=~2u;
        SteveBridge_RequestCommand(4,0,0);
    }
    if(sTestFrame==810) sTestGlide=sPose.flying &&
        (fabsf(GET_PLAYER(sPlay)->actor.world.pos.x-sTestFlightStart.x)+fabsf(GET_PLAYER(sPlay)->actor.world.pos.z-sTestFlightStart.z)>5);
    if(sTestFrame==900) {
        FILE* log=fopen("steve-in-game-test.log","w");
        int passed=sTestBasePassed && sTestTntPlaced && sTestTntPrimed && sTestTntExplosion>=2 &&
            sNativeAxeHits>=2 && sTestEnemyDamaged && sTestSwap && SteveUi_TestDraws()>3 && SteveUi_TextureCount()==10 && sTestGlide;
        if(log) {fprintf(log,"%s native SoH: rendered=%d mining=%llu TNT_place=%d TNT_prime=%d TNT_explode=%d axe_collisions=%d inventory_swap=%d inventory_draws=%d textures=%d elytra=%d fuse=%d charge_fuse=%d enemy_damaged=%d\n",
            passed?"PASS":"FAIL",sRenderedFrames,(unsigned long long)sPose.block_event,sTestTntPlaced,sTestTntPrimed,sTestTntExplosion,
            sNativeAxeHits,sTestSwap,SteveUi_TestDraws(),SteveUi_TextureCount(),sTestGlide,sPose.tnt_fuse,sPose.charges[0].fuse,sTestEnemyDamaged);fclose(log);}
        if(getenv("STEVE_IN_GAME_TEST_EXIT")) SteveActor_RequestExit();
    }
}
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
    if(sPlay==play) {int i;for(i=0;i<8;i++)if(sTntCollision[i] && sTnt[i].bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sTnt[i].bgId);}
    memset(sTntCollision,0,sizeof(sTntCollision));memset(sChargeToken,0,sizeof(sChargeToken));
    if(sAxeInitialized) {Collider_DestroyQuad(play,&sAxe);sAxeInitialized=0;}
    sTntAlive=0;sTntEvent=0;sAttackEvent=0;sWasFlying=0;
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
    if (sTestMode && sBlock.bgId < BG_ACTOR_MAX) sTestCollision=1;
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
        sSceneSerial++;
        sAnchorFrames=10;
        Collider_InitQuad(play,&sAxe);Collider_SetQuad(play,&sAxe,actor,&sAxeInit);sAxeInitialized=1;
    }
    /* Respawn/warp coordinates are applied after the first player update. */
    if (sAnchorFrames > 0) {
        if (sBlockAlive && sBlock.bgId < BG_ACTOR_MAX)
            DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sBlock.bgId);
        sBlockAlive=0; sLastTick=0;
        sOrigin=actor->world.pos; sSceneSerial++; sAnchorFrames--;
    }
    if (input->press.button & BTN_CRIGHT) sSlot = (sSlot + 1) % 9;
    if (input->press.button & BTN_CLEFT) sSlot = (sSlot + 8) % 9;
    packet.x = actor->world.pos.x / 20.0f;
    packet.y = actor->world.pos.y / 20.0f;
    packet.z = actor->world.pos.z / 20.0f;
    packet.yaw = actor->shape.rot.y * (3.14159265f / 32768.0f);
    packet.pitch = actor->focus.rot.x * (3.14159265f / 32768.0f);
    packet.slot = sSlot;
    packet.scene=(sSceneSerial<<16) | ((uint32_t)play->sceneNum & 65535u);
    packet.health_capacity=(uint32_t)gSaveContext.healthCapacity;
    if (sConnected && gSaveContext.health < sLastHealth && gSaveContext.healthCapacity > 0)
        packet.damage=(sLastHealth-gSaveContext.health)*20.0f/gSaveContext.healthCapacity;
    if (sConnected && sPose.health <= 0 && gSaveContext.health > 0) packet.flags=1;
    if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) packet.flags|=2;
    if (!(player->stateFlags1 & (PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_IN_CUTSCENE)) && !SteveUi_IsOpen()) {
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
        packet.target_enemy=player->focusActor->category==ACTORCAT_ENEMY;
        packet.target_token=(uint32_t)(((uintptr_t)player->focusActor>>4) ^ sSceneSerial);
        packet.target_health=player->focusActor->colChkInfo.health;
    }
    if(!packet.target_enemy && (sPose.item_kind==10 || sPose.item_kind==2)) {
        Actor* enemy=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
        Actor* nearest=NULL;float best=60*60;
        for(;enemy;enemy=enemy->next) {
            float dx=enemy->world.pos.x-actor->world.pos.x,dz=enemy->world.pos.z-actor->world.pos.z;
            float distance=dx*dx+dz*dz;
            if(enemy->update && enemy->colChkInfo.health && distance<best) {nearest=enemy;best=distance;}
        }
        if(nearest) {
            packet.locked=1;packet.target_enemy=1;packet.target_token=(uint32_t)(((uintptr_t)nearest>>4)^sSceneSerial);
            packet.target_health=nearest->colChkInfo.health;
            packet.tx=nearest->world.pos.x/20;packet.ty=nearest->world.pos.y/20;packet.tz=nearest->world.pos.z/20;
        }
    }
    /* The arena cube is a native dynamic collision object and the default mining target. */
    if (!packet.locked) {
        packet.tx=sOrigin.x/20.0f;
        packet.ty=(sOrigin.y+10)/20.0f;
        packet.tz=(sOrigin.z+40)/20.0f;
    }
    SteveActor_TestInput(&packet);
    sConnected = SteveBridge_Frame(&packet, &sPose);
    if (!sConnected && sBlockAlive) {
        if (sBlock.bgId < BG_ACTOR_MAX) DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sBlock.bgId);
        sBlockAlive=0;
    }
    if(!sConnected && sTntAlive) {
        int i;for(i=0;i<8;i++)if(sTntCollision[i]) {
            if(sTnt[i].bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sTnt[i].bgId);
            sTntCollision[i]=0;
        }
        sTntAlive=0;
    }
    if (sConnected && sPose.tick != sLastTick) {
        int sound;
        if(sTestMode && sTestTntExplosion==1) {
            Actor* explosive=play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;
            for(;explosive;explosive=explosive->next)if(explosive->id==ACTOR_EN_BOM && explosive->params==BOMB_EXPLOSION)sTestTntExplosion=2;
        }
        if (sPose.block_id && !sBlockAlive) SteveBlock_Create(play);
        sLastTick=sPose.tick;
        sBlockEvent=sPose.block_event;
        if (!sPose.block_id && sBlockAlive) {
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
        if(sPose.attack_event!=sAttackEvent) {
            Actor* enemy=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
            sAttackEvent=sPose.attack_event;
            for(;enemy;enemy=enemy->next) {
                uint32_t token=(uint32_t)(((uintptr_t)enemy>>4)^sSceneSerial);
                if(token==sPose.attack_target && enemy->update && sPose.attack_damage>0) {
                    Vec3f p[4];int i;
                    for(i=0;i<4;i++) {p[i]=enemy->world.pos;p[i].x+=(i&1)?20:-20;p[i].y+=(i&2)?50:-5;}
                    Collider_SetQuadVertices(&sAxe,&p[0],&p[1],&p[3],&p[2]);
                    sAxe.info.toucher.damage=(u8)fminf(255,ceilf(sPose.attack_damage/2));
                    CollisionCheck_SetAT(play,&play->colChkCtx,&sAxe.base);break;
                }
            }
        }
        {
            int i;sTntAlive=0;
            for(i=0;i<8;i++) {
                const SteveTnt* charge=&sPose.charges[i];DynaPolyActor* tnt=&sTnt[i];
                if(sTntCollision[i] && (charge->fuse!=-1 || sChargeToken[i]!=charge->token)) {
                    if(tnt->bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,tnt->bgId);sTntCollision[i]=0;
                }
                if(charge->token && charge->fuse==-1 && !sTntCollision[i]) {
                    memset(tnt,0,sizeof(*tnt));tnt->actor.id=ACTOR_BG_HEAVY_BLOCK;tnt->actor.update=SteveBlock_NoUpdate;
                    tnt->actor.world.pos.x=sOrigin.x+charge->x*20;tnt->actor.world.pos.y=sOrigin.y+charge->y*20+10;tnt->actor.world.pos.z=sOrigin.z+charge->z*20;
                    tnt->actor.scale.x=tnt->actor.scale.y=tnt->actor.scale.z=1;
                    DynaPolyActor_Init(tnt,DPM_UNK);tnt->bgId=DynaPoly_SetBgActor(play,&play->colCtx.dyna,&tnt->actor,&sBlockCollision);sTntCollision[i]=tnt->bgId<BG_ACTOR_MAX;
                }
                if(charge->token && charge->fuse==0 && (sChargeToken[i]!=charge->token || sChargeFuse[i]!=0)) {
                    Actor* bomb=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_BOM,sOrigin.x+charge->x*20,
                        sOrigin.y+charge->y*20+10,sOrigin.z+charge->z*20,0,0,0,0);
                    if(bomb){((EnBom*)bomb)->timer=0;sTestTntExplosion=1;}else sTestTntExplosion=-1;
                }
                sChargeToken[i]=charge->token;sChargeFuse[i]=charge->fuse;sTntAlive+=sTntCollision[i];
            }
            sTntEvent=sPose.tnt_event;
        }
    }
    sLastHealth=gSaveContext.health;
    if(SteveUi_IsOpen()) {memset(&input->cur,0,sizeof(input->cur));memset(&input->press,0,sizeof(input->press));}
    else if(sConnected) {
        if(sPose.item_kind==1 || sPose.item_kind==2 || sPose.item_kind==10) {
            input->cur.button&=~BTN_B;input->press.button&=~BTN_B;
        }
        if(sPose.item_kind==3 || sPose.item_kind==4 || sPose.item_kind==7 || sPose.item_kind==8) {
            input->cur.button&=~BTN_A;input->press.button&=~BTN_A;
        }
    }
}
void SteveActor_PostUpdate(Actor* actor,PlayState* play) {
    if(sConnected && sPose.flying) {
        if(!sWasFlying) sGravityBeforeFlight=actor->gravity;
        sWasFlying=1;
        actor->world.pos.x+=sPose.flight_x*20;actor->world.pos.y+=sPose.flight_y*20;actor->world.pos.z+=sPose.flight_z*20;
        actor->velocity.y=0;actor->gravity=0;actor->speedXZ=0;
        Actor_UpdateBgCheckInfo(play,actor,20,15,40,5);
    } else if(sWasFlying) {actor->gravity=sGravityBeforeFlight;sWasFlying=0;}
}

static void SteveSkinLimb(PlayState* play,int part,float x,float y,float z,float w,float h,float d) {
    int layer,face,i;
    const float corners[6][4][3] = {
        {{-w,h,-d},{w,h,-d},{w,h,d},{-w,h,d}},
        {{-w,-h,d},{w,-h,d},{w,-h,-d},{-w,-h,-d}},
        {{-w,-h,-d},{-w,-h,d},{-w,h,d},{-w,h,-d}},
        {{-w,-h,d},{w,-h,d},{w,h,d},{-w,h,d}},
        {{w,-h,d},{w,-h,-d},{w,h,-d},{w,h,d}},
        {{w,-h,-d},{-w,-h,-d},{-w,h,-d},{w,h,-d}}
    };
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push(); Matrix_Translate(x,y,z,MTXMODE_APPLY);
    gSPTexture(POLY_OPA_DISP++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    gDPSetTextureFilter(POLY_OPA_DISP++,G_TF_POINT);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_DECALRGBA,G_CC_DECALRGBA);
    gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_THRESHOLD);
    gDPSetBlendColor(POLY_OPA_DISP++,0,0,0,128);
    for(layer=0;layer<2;layer++) {
        Matrix_Push();
        if(layer) Matrix_Scale(1.0625f,1.0625f,1.0625f,MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        for(face=0;face<6;face++) {
            const SteveSkinFace* texture=&sSkinFaces[part][layer][face];
            Vtx* vertices=Graph_Alloc(play->state.gfxCtx,4*sizeof(Vtx));
            memset(vertices,0,4*sizeof(Vtx));
            for(i=0;i<4;i++) {
                int axis;
                for(axis=0;axis<3;axis++) vertices[i].v.ob[axis]=(s16)corners[face][i][axis];
                vertices[i].v.tc[0]=(i==1||i==2)?texture->width*32:0;
                vertices[i].v.tc[1]=(i<2)?texture->height*32:0;
                memset(vertices[i].v.cn,255,4);
            }
            gDPLoadTextureBlock(POLY_OPA_DISP++,(void*)texture->pixels,G_IM_FMT_RGBA,G_IM_SIZ_16b,
                texture->width,texture->height,0,G_TX_CLAMP,G_TX_CLAMP,G_TX_NOMASK,G_TX_NOMASK,G_TX_NOLOD,G_TX_NOLOD);
            gSPVertex(POLY_OPA_DISP++,(uintptr_t)vertices,4,0);
            gSP2Triangles(POLY_OPA_DISP++,0,1,2,0,0,2,3,0);
        }
        Matrix_Pop();
    }
    gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_NONE);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
static void SteveSprite(PlayState* play,const SteveSkinFace* texture,float x,float y,float z,float w,float h) {
    int i;
    Vtx* vertices=Graph_Alloc(play->state.gfxCtx,4*sizeof(Vtx));
    OPEN_DISPS(play->state.gfxCtx);
    memset(vertices,0,4*sizeof(Vtx));
    for(i=0;i<4;i++) {
        vertices[i].v.ob[0]=(i==1||i==2)?1:-1;
        vertices[i].v.ob[1]=(i<2)?-1:1;
        vertices[i].v.tc[0]=(i==1||i==2)?texture->width*32:0;
        vertices[i].v.tc[1]=(i<2)?texture->height*32:0;
        memset(vertices[i].v.cn,255,4);
    }
    Matrix_Push();Matrix_Translate(x,y,z,MTXMODE_APPLY);Matrix_Scale(w,h,1,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPTexture(POLY_OPA_DISP++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    gDPSetTextureFilter(POLY_OPA_DISP++,G_TF_POINT);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_DECALRGBA,G_CC_DECALRGBA);
    gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_THRESHOLD);gDPSetBlendColor(POLY_OPA_DISP++,0,0,0,128);
    gDPLoadTextureBlock(POLY_OPA_DISP++,(void*)texture->pixels,G_IM_FMT_RGBA,G_IM_SIZ_16b,
        texture->width,texture->height,0,G_TX_CLAMP,G_TX_CLAMP,G_TX_NOMASK,G_TX_NOMASK,G_TX_NOLOD,G_TX_NOLOD);
    gSPVertex(POLY_OPA_DISP++,(uintptr_t)vertices,4,0);
    gSP2Triangles(POLY_OPA_DISP++,0,1,2,0,0,2,3,0);
    gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_NONE);
    Matrix_Pop();CLOSE_DISPS(play->state.gfxCtx);
}
static void SteveTexturedCube(PlayState* play,float x,float y,float z,const SteveSkinFace* side,
                              const SteveSkinFace* top,const SteveSkinFace* bottom) {
    int face;
    Matrix_Push();Matrix_Translate(x,y,z,MTXMODE_NEW);
    for(face=0;face<4;face++) {
        Matrix_Push();Matrix_RotateY(face*1.5707963f,MTXMODE_APPLY);
        SteveSprite(play,side,0,0,10,10,10);Matrix_Pop();
    }
    Matrix_Push();Matrix_RotateX(-1.5707963f,MTXMODE_APPLY);SteveSprite(play,top,0,0,10,10,10);Matrix_Pop();
    Matrix_Push();Matrix_RotateX(1.5707963f,MTXMODE_APPLY);SteveSprite(play,bottom,0,0,10,10,10);Matrix_Pop();
    Matrix_Pop();
}
int SteveActor_Draw(Actor* actor, PlayState* play) {
    if (!sConnected || !CVarGetInteger("gEnhancements.SteveBridge",1)) return 0;
    if (!SteveBridge_GetPose(&sPose)) return 0;
    sRenderedFrames++;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768.0f),MTXMODE_APPLY);
    if(sPose.flying) {Matrix_Translate(0,15,0,MTXMODE_APPLY);Matrix_RotateX(1.5707963f,MTXMODE_APPLY);}
    Matrix_Scale(1.125f,1.125f,1.125f,MTXMODE_APPLY);
    Matrix_Push(); Matrix_Translate(0,28,0,MTXMODE_APPLY);
    Matrix_RotateX(actor->focus.rot.x*(3.14159265f/32768.0f),MTXMODE_APPLY);
    SteveSkinLimb(play,0,0,0,0,4,4,4); Matrix_Pop();
    SteveSkinLimb(play,1,0,18,0,4,6,2);
    {
        float gait=sinf(play->gameplayFrames*0.3f)*fminf(fabsf(actor->speedXZ)*0.16f,0.8f);
        Matrix_Push();
        Matrix_Translate(-5,22,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.right_arm+gait,MTXMODE_APPLY);
        SteveSkinLimb(play,2,-1,-4,0,2,6,2);
        if(sPose.item_kind>0 && sPose.item_kind<11)
            SteveSprite(play,&sItemFaces[sPose.item_kind],0,-9,3,4,4);
        Matrix_Pop();
        Matrix_Push();
        Matrix_Translate(5,22,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.left_arm-gait,MTXMODE_APPLY);
        SteveSkinLimb(play,3,1,-4,0,2,6,2);
        if (sPose.hand==3) SteveSprite(play,&sShieldFace,0,-6,3,4,6);
        Matrix_Pop();
        Matrix_Push(); Matrix_Translate(-2,12,0,MTXMODE_APPLY);
        Matrix_RotateX(-gait,MTXMODE_APPLY); SteveSkinLimb(play,4,0,-6,0,2,6,2); Matrix_Pop();
        Matrix_Push(); Matrix_Translate(2,12,0,MTXMODE_APPLY);
        Matrix_RotateX(gait,MTXMODE_APPLY); SteveSkinLimb(play,5,0,-6,0,2,6,2); Matrix_Pop();
    }
    if(sPose.chest.id) {
        float spread=sPose.flying?0.9f:0.2f;
        Matrix_Push(); Matrix_Translate(-2,20,-2.2f,MTXMODE_APPLY);
        Matrix_RotateZ(spread,MTXMODE_APPLY);SteveSprite(play,&sElytraWing,-4,-8,0,5,10);Matrix_Pop();
        Matrix_Push(); Matrix_Translate(2,20,-2.2f,MTXMODE_APPLY);
        Matrix_RotateZ(-spread,MTXMODE_APPLY);SteveSprite(play,&sElytraWing,4,-8,0,5,10);Matrix_Pop();
    }
    Matrix_Pop();
    if (sBlockAlive) {
        SteveTexturedCube(play,sBlock.actor.world.pos.x,sBlock.actor.world.pos.y,sBlock.actor.world.pos.z,
            &sStoneFace,&sStoneFace,&sStoneFace);
    }
    {
        int i;for(i=0;i<8;i++) {
            const SteveTnt* charge=&sPose.charges[i];
            if(charge->token && charge->fuse>=-1 && charge->fuse!=0)
                SteveTexturedCube(play,sOrigin.x+charge->x*20,sOrigin.y+charge->y*20+10,sOrigin.z+charge->z*20,
                    &sItemFaces[7],&sTntTop,&sTntBottom);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
    {
        GfxPrint printer;
        int i;
        OPEN_DISPS(play->state.gfxCtx);
        GfxPrint_Init(&printer);
        GfxPrint_Open(&printer,OVERLAY_DISP);
        GfxPrint_SetColor(&printer,255,255,255,255);
        GfxPrint_SetPos(&printer,2,22);
        GfxPrint_Printf(&printer,"MC ");
        for(i=0;i<9;i++) {
            if ((uint32_t)i==sPose.selected_slot) GfxPrint_SetColor(&printer,255,215,60,255);
            else GfxPrint_SetColor(&printer,190,190,190,255);
            GfxPrint_Printf(&printer,"%d ",(int)sPose.hotbar[i].count);
        }
        GfxPrint_SetColor(&printer,255,255,255,255);
        GfxPrint_SetPos(&printer,2,23);
        GfxPrint_Printf(&printer,"Slot %u  Dur %u  Mine %d",sPose.selected_slot+1,sPose.hotbar[sPose.selected_slot%9].durability,sPose.destruction_stage);
        OVERLAY_DISP=GfxPrint_Close(&printer);
        GfxPrint_Destroy(&printer);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return 1;
}
