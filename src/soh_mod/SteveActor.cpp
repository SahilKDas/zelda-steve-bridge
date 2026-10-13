/* In-tree SoH adapter. This file needs SoH's global.h and libultraship. */
#include "global.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "soh_mod/soh_api.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iterator>
#include "soh_mod/generated/minecraft_textures.inc"
static bool sFirstPersonPass;
static Gfx*& SteveDisplayList(GraphicsContext* context){return sFirstPersonPass?context->overlay.p:context->polyOpa.p;}
#undef POLY_OPA_DISP
#define POLY_OPA_DISP SteveDisplayList(__gfxCtx)
extern "C" {

static StevePose sPose;
static int sConnected;
static uint32_t sSlot;
static int sJumpRequested,sUseRequested,sMouseMining,sMouseUsing,sMineRequested;
void SteveActor_SetMining(int down){sMouseMining=down;if(down)sMineRequested=1;}
void SteveActor_SetUsing(int down){sMouseUsing=down;}
void SteveActor_RequestUse(void) {sUseRequested=1;}
void SteveActor_RequestJump(void) {sJumpRequested=1;}
void StevePlayer_Jump(Player* player,PlayState* play);
void SteveActor_SelectSlot(uint32_t slot) {sSlot=slot%10;}
static uint32_t sSceneSerial;
static int sAnchorFrames;
int SteveUi_IsOpen(void);
void SteveActor_RequestExit(void);
void SteveUi_TestInventory(int open);
void SteveTerrain_Sync(const StevePose* pose);
int SteveTerrain_Contains(float x,float y,float z);
int SteveTerrain_ClippedTriangles(void);
int SteveUi_TestDraws(void);
int SteveUi_TextureCount(void);
int SteveUi_TestMouseClick(int button);
int SteveUi_TestMouseHeld(int button,int pressed);
int SteveUi_TestAimDraws(void);
int SteveActor_IsReady(void) { return sConnected && CVarGetInteger("gEnhancements.SteveBridge",1) && SteveBridge_GetPose(&sPose); }
static PlayState* sPlay;
int SteveActor_HudAllowed(void) {
    return sPlay && sConnected && gSaveContext.gameMode==GAMEMODE_NORMAL && sPlay->csCtx.state==CS_STATE_IDLE && sPlay->msgCtx.msgMode==MSGMODE_NONE &&
        sPlay->pauseCtx.state==0 && !(GET_PLAYER(sPlay)->stateFlags1&PLAYER_STATE1_IN_CUTSCENE);
}
static Vec3f sOrigin;
static float sLookYaw,sLookPitch;
static ActorShadowFunc sShadowBeforeFirstPerson;
static bool sShadowMasked;
static int sLookReady,sAimValid,sFirstPersonDraws;
static uint32_t sHandItemKind;
static float sEquipProgress=1;
static Vec3f sAimMine,sAimPlace;
int SteveActor_FirstPerson(void){return SteveActor_HudAllowed() && CVarGetInteger("gEnhancements.SteveFirstPerson",1);}
void SteveActor_Look(float dx,float dy){if(!sLookReady)return;sLookYaw-=dx*0.0025f;sLookPitch-=dy*0.0025f;sLookPitch=fmaxf(-1.569f,fminf(1.569f,sLookPitch));}
int SteveActor_GetAim(int placing,float* x,float* y,float* z){if(!sAimValid)return 0;Vec3f p=placing?sAimPlace:sAimMine;*x=p.x;*y=p.y;*z=p.z;return 1;}
int SteveActor_Project(float x,float y,float z,float aspect,float* u,float* v){
    if(!sPlay)return 0;
    Camera* camera=GET_ACTIVE_CAM(sPlay);Vec3f f={camera->at.x-camera->eye.x,camera->at.y-camera->eye.y,camera->at.z-camera->eye.z};
    float length=sqrtf(f.x*f.x+f.y*f.y+f.z*f.z);if(length<0.001f)return 0;f.x/=length;f.y/=length;f.z/=length;
    float horizontal=sqrtf(f.x*f.x+f.z*f.z);if(horizontal<0.001f)return 0;
    Vec3f right={-f.z/horizontal,0,f.x/horizontal},up={-f.y*f.x/horizontal,horizontal,-f.y*f.z/horizontal};
    Vec3f d={x-camera->eye.x,y-camera->eye.y,z-camera->eye.z};float depth=d.x*f.x+d.y*f.y+d.z*f.z;if(depth<1.f)return 0;
    float tangent=tanf(camera->fov*0.008726646f);
    *u=0.5f+(d.x*right.x+d.z*right.z)/(2*depth*tangent*aspect);
    *v=0.5f-(d.x*up.x+d.y*up.y+d.z*up.z)/(2*depth*tangent);return 1;
}
void SteveCamera_Update(PlayState* play){
    if(!SteveActor_FirstPerson() || !sLookReady)return;
    Player* player=GET_PLAYER(play);Camera* camera=GET_ACTIVE_CAM(play);
    camera->eye=player->actor.world.pos;camera->eye.y+=sPose.flying?8.f:32.4f;
    camera->at={camera->eye.x+sinf(sLookYaw)*cosf(sLookPitch)*100,camera->eye.y+sinf(sLookPitch)*100,camera->eye.z+cosf(sLookYaw)*cosf(sLookPitch)*100};
    camera->eyeNext=camera->eye;camera->up={0,1,0};camera->fov=70;
    camera->inputDir.y=camera->camDir.y=(s16)(sLookYaw*32768/3.14159265f);camera->inputDir.x=camera->camDir.x=(s16)(sLookPitch*32768/3.14159265f);
    play->view.fovy=70;func_800AA358(&play->view,&camera->eye,&camera->at,&camera->up);
}

static uint64_t sLastTick, sBlockEvent;
static int sLastHealth, sBlockAlive;
static DynaPolyActor sBlock;
static DynaPolyActor sTnt[8];
static int sTntCollision[8];
static uint64_t sChargeToken[8];
static DynaPolyActor sCells[128];
static int sCellCollision[128];
static uint64_t sCellToken[128];
static int sChargeFuse[8];
static int sTntAlive;
static uint64_t sTntEvent,sAttackEvent;
static ColliderQuad sAxe;
static int sAxeInitialized;
static int sWasFlying;
static float sWingSpread=0.2618f,sWingPitch=0.2618f;
static float sGravityBeforeFlight;
static int sNativeAxeHits;
static int sArmorGlintDraws;
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
static float sJumpTestPeak,sMouseFlightBefore;
static int sJumpTestPlaced,sJumpTestLanded,sMouseGlide,sMouseBoost,sMouseReserved,sMouseMined;
static uint32_t sTestPick, sTestFood, sTestBow, sTestArrows;
static int sTestShield, sTestDamage, sTestCollision;
static int sTestBasePassed,sTestTntPlaced,sTestTntPrimed,sTestTntExplosion,sTestSwap,sTestGlide;
static uint32_t sTestEnemyToken;
static int sTestEnemyHealth,sTestEnemyDamaged;
static Vec3f sTestFlightStart,sTestTerrain;
static float sTestGroundBefore;
static int sTestTerrainCut,sTestObsidianPlaced,sTestObsidianRecovered;
static void SteveActor_TestInput(SteveInput* packet) {
    if (sTestMode < 0) sTestMode=getenv("STEVE_JUMP_TEST")?2:(getenv("STEVE_IN_GAME_TEST") != NULL);
    if(sTestMode==2) return;
    if (!sTestMode) return;
    packet->buttons=0; packet->locked=0; packet->slot=1;
    if (!sConnected) { packet->flags|=1; return; }
    sTestFrame++;
    if (sTestFrame==175) sTestShield=sPose.hand==3;
    if (sTestFrame==210) sTestDamage=sPose.health<20;
    if (sTestFrame==1) {
        CVarSetInteger("gEnhancements.SteveFirstPerson",0);
        packet->flags|=1;
        sTestPick=sPose.hotbar[1].durability; sTestFood=sPose.hotbar[2].count;
        sTestBow=sPose.hotbar[3].durability; sTestArrows=sPose.hotbar[8].count;
    }
    if (sTestFrame>=100 && sTestFrame<160) packet->buttons=2;
    else if (sTestFrame>=160 && sTestFrame<180) packet->buttons=8;
    else if (sTestFrame==190) packet->damage=4;
    else if (sTestFrame>=220 && sTestFrame<300) {packet->slot=2;packet->buttons=1;}
    else if (sTestFrame>=310 && sTestFrame<350) packet->slot=3;
    else if (sTestFrame>=350) packet->slot=3;
    if (sTestFrame==380) {
        FILE* log=fopen("steve-in-game-test.log","w");
        int passed=sRenderedFrames>20 && sTestCollision && !sBlockAlive && sTestShield && sTestDamage &&
            sPose.hotbar[1].durability==sTestPick-1 && sPose.hotbar[2].count==sTestFood &&
            sPose.hotbar[3].durability==sTestBow && sPose.hotbar[8].count==sTestArrows;
        if (log) {
            fprintf(log,"%s native SoH actor: rendered=%d tick=%llu mining_ack=%llu block_alive=%d pickaxe=%u food=%u bow=%u arrows=%u health=%.2f\n",
                passed?"PASS":"FAIL",sRenderedFrames,(unsigned long long)sPose.tick,
                (unsigned long long)sPose.block_event,sBlockAlive,sPose.hotbar[1].durability,
                sPose.hotbar[2].count,sPose.hotbar[3].durability,sPose.hotbar[8].count,sPose.health);
            fclose(log);
        }
        sTestBasePassed=passed;
    }
    if(sTestFrame>=390 && sTestFrame<395) {packet->slot=9;packet->buttons=1;}
    if(sTestFrame==410) sTestTntPlaced=sTntAlive && sPose.inventory[9].count==64;
    if(sTestFrame>=415 && sTestFrame<420) {packet->slot=3;packet->buttons=1;}
    if(sTestFrame==435) sTestTntPrimed=sPose.tnt_fuse>0 && sPose.hotbar[3].durability==63;
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
        if(sTestFrame==670){sTestEnemyDamaged=sTestEnemyHealth>0 && (!enemy || enemy->colChkInfo.health<sTestEnemyHealth);if(enemy)Actor_Kill(enemy);}
    }
    if(sTestFrame>=525 && sTestFrame<650) {packet->slot=0;if(sTestFrame%20<5)packet->buttons=2;}
    if(sTestFrame==680) SteveBridge_RequestCommand(1,0,10);
    if(sTestFrame==710) {sTestSwap=sPose.inventory_kind[10]==10;SteveUi_TestInventory(1);}
    if(sTestFrame==735) {SteveUi_TestInventory(0);SteveBridge_RequestCommand(1,0,10);}
    if(sTestFrame==760) SteveBridge_RequestCommand(2,6,0);
    if(sTestFrame==780) {
        GET_PLAYER(sPlay)->actor.world.pos.y+=120;
        sTestFlightStart=GET_PLAYER(sPlay)->actor.world.pos;
        GET_PLAYER(sPlay)->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;
        packet->y=GET_PLAYER(sPlay)->actor.world.pos.y/20;packet->flags&=~2u;
        SteveActor_RequestJump();
    }
    if(sTestFrame==810) sTestGlide=sPose.flying &&
        (fabsf(GET_PLAYER(sPlay)->actor.world.pos.x-sTestFlightStart.x)+fabsf(GET_PLAYER(sPlay)->actor.world.pos.z-sTestFlightStart.z)>5);
    if(sTestFrame==850)CVarSetInteger("gEnhancements.SteveFirstPerson",1);
    if(sTestFrame>=890) {
        GET_PLAYER(sPlay)->actor.world.pos=sOrigin;GET_PLAYER(sPlay)->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
        GET_PLAYER(sPlay)->invincibilityTimer=30;
        packet->x=sOrigin.x/20;packet->y=sOrigin.y/20;packet->z=sOrigin.z/20;packet->flags|=2;
    }
    if(sTestFrame==930) {
        Vec3f query=GET_PLAYER(sPlay)->actor.world.pos;query.x+=40;query.y+=60;CollisionPoly* poly=NULL;
        sTestGroundBefore=BgCheck_EntityRaycastFloor2(sPlay,&sPlay->colCtx,&poly,&query);
        sTestTerrain={floorf(query.x/20)+0.5f,floorf((sTestGroundBefore-1)/20)+0.5f,floorf(query.z/20)+0.5f};
    }
    if(sTestFrame>=930 && sTestFrame<1030) {
        packet->slot=1;packet->buttons=2;packet->flags|=4;packet->locked=1;packet->target_enemy=0;
        packet->tx=sTestTerrain.x;packet->ty=sTestTerrain.y;packet->tz=sTestTerrain.z;
    }
    if(sTestFrame==1070) {
        Vec3f query={sTestTerrain.x*20,sTestGroundBefore+60,sTestTerrain.z*20};CollisionPoly* poly=NULL;s32 bgId;
        float after=BgCheck_EntityRaycastFloor5(sPlay,&sPlay->colCtx,&poly,&bgId,NULL,&query);
        int masked=SteveTerrain_Contains(query.x,sTestGroundBefore,query.z);
        sTestTerrainCut=masked && after<sTestGroundBefore-0.5f && SteveTerrain_ClippedTriangles()>0;
        FILE* trace=fopen("steve-native-trace.log","a");if(trace){fprintf(trace,"floor before=%.2f after=%.2f masked=%d bg=%d cell0=%.2f/%.2f/%.2f active=%u flags=%u collision=%d cubeY=%.2f\n",
            sTestGroundBefore,after,masked,bgId,sPose.cells[0].x,sPose.cells[0].y,sPose.cells[0].z,sPose.cells[0].active,sPose.cells[0].flags,sCellCollision[0],sCells[0].actor.world.pos.y);fclose(trace);}
    }
    if(sTestFrame>=1100 && sTestFrame<1105) {
        packet->slot=5;packet->buttons=1;packet->flags|=8;packet->tx=sTestTerrain.x;packet->ty=sTestTerrain.y+1;packet->tz=sTestTerrain.z;
    }
    if(sTestFrame==1130)for(int i=0;i<128;i++)if(sPose.cells[i].active && sPose.cells[i].kind==13)sTestObsidianPlaced=sPose.inventory[5].count==31;
    if(sTestFrame>=1140 && sTestFrame<1145) {
        packet->slot=0;packet->buttons=2;packet->flags|=4;packet->locked=1;packet->target_enemy=0;
        packet->tx=sTestTerrain.x;packet->ty=sTestTerrain.y+1;packet->tz=sTestTerrain.z;
    }
    if(sTestFrame==1170)sTestObsidianRecovered=sPose.inventory[5].count==32;
    if(sTestFrame==550 || sTestFrame==710 || sTestFrame==940 || sTestFrame==1070 || sTestFrame==1130) {
        FILE* trace=fopen("steve-native-trace.log","a");
        if(trace){fprintf(trace,"frame=%d pos=%.2f/%.2f/%.2f target=%.2f/%.2f/%.2f flags=%u enemy=%u token=%u kind=%u slot0=%u slot10=%u health=%.2f attack=%llu damage=%.2f ground=%.2f\n",
            sTestFrame,packet->x,packet->y,packet->z,packet->tx,packet->ty,packet->tz,packet->flags,packet->target_enemy,packet->target_token,
            sPose.item_kind,sPose.inventory_kind[0],sPose.inventory_kind[10],sPose.health,(unsigned long long)sPose.attack_event,sPose.attack_damage,sTestGroundBefore);fclose(trace);}
    }
    if(sTestFrame==1200) {
        FILE* log=fopen("steve-in-game-test.log","w");
        int passed=sTestBasePassed && sTestTntPlaced && sTestTntPrimed && sTestTntExplosion>=2 &&
            sNativeAxeHits>=2 && sTestEnemyDamaged && sTestSwap && SteveUi_TestDraws()>3 && SteveUi_TextureCount()==14 && sTestGlide && sArmorGlintDraws>0 && sTestTerrainCut && sTestObsidianPlaced && sTestObsidianRecovered && sFirstPersonDraws>20;
        if(log) {fprintf(log,"%s native SoH: rendered=%d mining=%llu TNT_place=%d TNT_prime=%d TNT_explode=%d axe_collisions=%d inventory_swap=%d inventory_draws=%d textures=%d elytra=%d fuse=%d charge_fuse=%d enemy_damaged=%d terrain=%d clipped=%d obsidian=%d/%d first_person=%d\n",
            passed?"PASS":"FAIL",sRenderedFrames,(unsigned long long)sPose.block_event,sTestTntPlaced,sTestTntPrimed,sTestTntExplosion,
            sNativeAxeHits,sTestSwap,SteveUi_TestDraws(),SteveUi_TextureCount(),sTestGlide,sPose.tnt_fuse,sPose.charges[0].fuse,sTestEnemyDamaged,sTestTerrainCut,SteveTerrain_ClippedTriangles(),sTestObsidianPlaced,sTestObsidianRecovered,sFirstPersonDraws);fclose(log);}
        if(getenv("STEVE_IN_GAME_TEST_EXIT")) SteveActor_RequestExit();
    }
}
static Vec3s sBlockVertices[] = {
    {-10,-10,-10},{10,-10,-10},{10,10,-10},{-10,10,-10},
    {-10,-10,10},{10,-10,10},{10,10,10},{-10,10,10}
};
static CollisionPoly sBlockPolys[12];
static SurfaceType sBlockSurface[] = {{{0,0}}};
// Ground-camera queries index this list even for surfaces with no camera override.
static CamData sBlockCamera[] = {{CAM_SET_NONE,0,NULL}};
static CollisionHeader sBlockCollision = {
    {-10,-10,-10},{10,10,10},8,sBlockVertices,12,sBlockPolys,sBlockSurface,sBlockCamera,0,NULL,0
};
static void SteveBlock_NoUpdate(Actor* actor, PlayState* play) { (void)actor; (void)play; }
void SteveActor_Reset(PlayState* play) {
    for(int i=0;i<128;i++)if(sCellCollision[i]){if(sCells[i].bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sCells[i].bgId);sCellCollision[i]=0;sCellToken[i]=0;}
    SteveTerrain_Sync(NULL);
    if (sPlay == play && sBlockAlive && sBlock.bgId < BG_ACTOR_MAX)
        DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sBlock.bgId);
    if(sPlay==play) {int i;for(i=0;i<8;i++)if(sTntCollision[i] && sTnt[i].bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sTnt[i].bgId);}
    memset(sTntCollision,0,sizeof(sTntCollision));memset(sChargeToken,0,sizeof(sChargeToken));
    if(sAxeInitialized) {Collider_DestroyQuad(play,&sAxe);sAxeInitialized=0;}
    sTntAlive=0;sTntEvent=0;sAttackEvent=0;sWasFlying=0;sMouseMining=0;sMouseUsing=0;
    sShadowMasked=false;sLookReady=0;sAimValid=0;sPlay = NULL; sBlockAlive = 0; sConnected = 0; sLastTick = 0;
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
    if(sShadowMasked){actor->shape.shadowDraw=sShadowBeforeFirstPerson;sShadowMasked=false;}
    Player* player = (Player*)actor;
    Input* input = &play->state.input[0];
    SteveInput packet = {0};
    if (!CVarGetInteger("gEnhancements.SteveBridge", 1)) { if (sPlay) SteveActor_Reset(sPlay); return; }
    if (sPlay != play) {
        sLookYaw=actor->shape.rot.y*(3.14159265f/32768);sLookPitch=0;sLookReady=1;
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
    if(getenv("STEVE_JUMP_TEST")) {
        sTestMode=2;
        if(sConnected) {
            sTestFrame++;sSlot=sTestFrame>=270?1u:(sTestFrame>=205?7u:4u);
            if(sTestFrame==20){actor->world.pos.x=5;actor->world.pos.z=5;sOrigin=actor->world.pos;}
            if(sTestFrame==40)sMouseReserved=SteveUi_TestMouseClick(1);
            if(sTestFrame>=265 && sTestFrame<=270)actor->bgCheckFlags|=BGCHECKFLAG_GROUND;
            if(sTestFrame>=270 && sTestFrame<=275){sLookPitch=-1.4f;actor->world.pos={5,20,5};actor->prevPos=actor->world.pos;actor->velocity={0,0,0};actor->floorHeight=20;actor->bgCheckFlags|=BGCHECKFLAG_GROUND;}
            if(sTestFrame>=270 && sTestFrame<=320){actor->world.pos.x=5;actor->world.pos.z=5;actor->prevPos.x=5;actor->prevPos.z=5;actor->velocity.x=0;actor->velocity.z=0;actor->speedXZ=0;player->invincibilityTimer=30;}
            if(sTestFrame==275)SteveUi_TestMouseHeld(0,1);
            if(sTestFrame==320)SteveUi_TestMouseHeld(0,0);
            if(sTestFrame==340)sMouseMined=std::none_of(std::begin(sPose.cells),std::end(sPose.cells),[](const SteveCell& cell){return cell.active && cell.kind==5;});
            if(sTestFrame==185)SteveBridge_RequestCommand(2,6,0);
            if(sTestFrame==200){actor->world.pos.y+=500;actor->bgCheckFlags&=~BGCHECKFLAG_GROUND;}
            if(sTestFrame==201)SteveActor_RequestJump();
            if(sTestFrame==210){sMouseGlide=sPose.flying;sMouseFlightBefore=sqrtf(sPose.flight_x*sPose.flight_x+sPose.flight_y*sPose.flight_y+sPose.flight_z*sPose.flight_z);}
            if(sTestFrame==215)sMouseReserved&=SteveUi_TestMouseClick(2);
            if(sTestFrame>=220 && sTestFrame<245 && sqrtf(sPose.flight_x*sPose.flight_x+sPose.flight_y*sPose.flight_y+sPose.flight_z*sPose.flight_z)>sMouseFlightBefore+0.1f)sMouseBoost=1;
            if(sTestFrame==185 || sTestFrame==200 || sTestFrame==201 || sTestFrame==210 || sTestFrame==215 || sTestFrame==225 || sTestFrame==275 || sTestFrame==290 || sTestFrame==310) {
                FILE* trace=fopen("steve-mouse-trace.log","a");
                if(trace){fprintf(trace,"frame=%d y=%.2f grounded=%u hud=%d chest=%u equip=%u fly=%u kind=%u ack=%llu result=%u use=%d\n",
                    sTestFrame,actor->world.pos.y,(unsigned)(actor->bgCheckFlags&BGCHECKFLAG_GROUND),SteveActor_HudAllowed(),sPose.chest.id,
                    sPose.equipment_flags,sPose.flying,sPose.item_kind,(unsigned long long)sPose.command_ack,sPose.command_result,sUseRequested);fclose(trace);}
            }
            input->cur.button=(sTestFrame>=46 && sTestFrame<=50)?BTN_A:0;
        }
    }
    const int jumpRequested=sJumpRequested;
    sJumpRequested=0;
    if(jumpRequested && sConnected && SteveActor_HudAllowed() && !SteveUi_IsOpen() &&
       !(actor->bgCheckFlags&BGCHECKFLAG_GROUND) && !sPose.flying)SteveBridge_RequestCommand(4,0,0);
    if(jumpRequested && sConnected && SteveActor_HudAllowed() && !SteveUi_IsOpen() &&
       !sPose.flying && (actor->bgCheckFlags&BGCHECKFLAG_GROUND) &&
       !(player->stateFlags1&(PLAYER_STATE1_INPUT_DISABLED|PLAYER_STATE1_ON_HORSE))) {
        StevePlayer_Jump(player,play);
    }
    if (input->press.button & BTN_CRIGHT) sSlot = (sSlot + 1) % 10;
    if (input->press.button & BTN_CLEFT) sSlot = (sSlot + 9) % 10;
    packet.x = actor->world.pos.x / 20.0f;
    packet.y = actor->world.pos.y / 20.0f;
    packet.z = actor->world.pos.z / 20.0f;
    packet.yaw = actor->shape.rot.y * (3.14159265f / 32768.0f);
    packet.pitch = actor->focus.rot.x * (3.14159265f / 32768.0f);
    if(SteveActor_FirstPerson()){packet.yaw=sLookYaw;packet.pitch=-sLookPitch;}
    sAimValid=0;
    packet.slot = sSlot;
    packet.scene=(sSceneSerial<<16) | ((uint32_t)play->sceneNum & 65535u);
    packet.health_capacity=(uint32_t)gSaveContext.healthCapacity;
    if (sConnected && gSaveContext.health < sLastHealth && gSaveContext.healthCapacity > 0)
        packet.damage=(sLastHealth-gSaveContext.health)*20.0f/gSaveContext.healthCapacity;
    if(packet.damage>0 && gSaveContext.health<=0 && sPose.health>0)gSaveContext.health=1;
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
    if(sUseRequested && sConnected && SteveActor_HudAllowed() && !SteveUi_IsOpen())packet.buttons|=1;
    sUseRequested=0;
    if(sMineRequested && SteveActor_HudAllowed() && !SteveUi_IsOpen())packet.buttons|=2;sMineRequested=0;
    if(SteveActor_HudAllowed() && !SteveUi_IsOpen()){if(sMouseMining)packet.buttons|=2;if(sMouseUsing)packet.buttons|=1;}
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
            if(SteveActor_FirstPerson() && distance>1 && (dx*sinf(sLookYaw)+dz*cosf(sLookYaw))<sqrtf(distance)*0.8f)continue;
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
    if(sTestMode!=1 && !(player->stateFlags1&PLAYER_STATE1_IN_CUTSCENE) && !packet.target_enemy) {
        Camera* camera=GET_ACTIVE_CAM(play);
        Vec3f from=actor->world.pos,to,hit;
        CollisionPoly* poly=NULL; s32 bgId;
        from.y+=SteveActor_FirstPerson()?(sPose.flying?8.f:32.4f):22.f;
        float dx=camera->at.x-camera->eye.x,dy=camera->at.y-camera->eye.y,dz=camera->at.z-camera->eye.z;
        float length=sqrtf(dx*dx+dy*dy+dz*dz);
        if(length>0.001f){dx/=length;dy/=length;dz/=length;}else{dx=sinf(packet.yaw);dy=-0.5f;dz=cosf(packet.yaw);}
        // A level third-person camera still aims at the ground ahead of the feet.
        if(SteveActor_FirstPerson()){dx=sinf(sLookYaw)*cosf(sLookPitch);dy=sinf(sLookPitch);dz=cosf(sLookYaw)*cosf(sLookPitch);}
        else if(dy>-0.25f)dy=-0.35f;
        to.x=from.x+dx*100;to.y=from.y+dy*100;to.z=from.z+dz*100;
        if(BgCheck_EntityLineTest1(&play->colCtx,&from,&to,&hit,&poly,true,true,true,true,&bgId)) {
            float nx=COLPOLY_GET_NORMAL(poly->normal.x),ny=COLPOLY_GET_NORMAL(poly->normal.y),nz=COLPOLY_GET_NORMAL(poly->normal.z);
            int placing=(packet.buttons&1) && (sPose.item_kind==5 || sPose.item_kind==13 || sPose.item_kind==7);
            sAimMine={floorf((hit.x-nx)/20)*20+10,floorf((hit.y-ny)/20)*20+10,floorf((hit.z-nz)/20)*20+10};
            sAimPlace={floorf((hit.x+nx)/20)*20+10,floorf((hit.y+ny)/20)*20+10,floorf((hit.z+nz)/20)*20+10};sAimValid=1;
            float offset=placing?1.f:-1.f;
            packet.tx=floorf((hit.x+nx*offset)/20)+0.5f;
            packet.ty=floorf((hit.y+ny*offset)/20)+0.5f;
            packet.tz=floorf((hit.z+nz*offset)/20)+0.5f;
            packet.locked=1;packet.flags|=placing?8u:4u;
            for(int i=0;i<128;i++)if(sCellCollision[i] && sCells[i].bgId==bgId) {
                const SteveCell* cell=&sPose.cells[i];
                if(!placing){packet.tx=cell->x;packet.ty=cell->y-((cell->flags&1)?1.f:0.f);packet.tz=cell->z;sAimMine={packet.tx*20,packet.ty*20,packet.tz*20};}
                break;
            }
        }
    }
    // While airborne, place on the supporting surface directly beneath Steve.
    // Require space above its top so placement cannot intersect his feet.
    if(sTestMode!=1 && !(actor->bgCheckFlags&BGCHECKFLAG_GROUND) &&
       !packet.target_enemy &&
       (sPose.item_kind==5 || sPose.item_kind==13 || sPose.item_kind==7)) {
        Vec3f from=actor->world.pos,to=from,hit;
        CollisionPoly* poly=NULL; s32 bgId;
        to.y-=80;
        if(BgCheck_EntityLineTest1(&play->colCtx,&from,&to,&hit,&poly,false,true,false,true,&bgId)) {
            float centerY=floorf((hit.y+1.f)/20.f)*20.f+10.f;
            if(actor->world.pos.y>=centerY+12.f) {
                sAimPlace={floorf(actor->world.pos.x/20.f)*20+10,centerY,floorf(actor->world.pos.z/20.f)*20+10};sAimValid=1;
                if(packet.buttons&1){packet.tx=sAimPlace.x/20;packet.ty=sAimPlace.y/20;packet.tz=sAimPlace.z/20;packet.locked=1;packet.flags=(packet.flags&~4u)|8u;}
            }
        }
    }
    if(sTestMode==0 && !sAimValid && !packet.target_enemy){if(sPose.item_kind==1)packet.buttons&=~2u;if(sPose.item_kind==5 || sPose.item_kind==13)packet.buttons&=~1u;}
    if(sTestMode==2 && (sTestFrame==275 || sTestFrame==290 || sTestFrame==310)){
        FILE* trace=fopen("steve-mouse-trace.log","a");if(trace){fprintf(trace,"aim frame=%d pos=%.2f/%.2f/%.2f target=%.2f/%.2f/%.2f buttons=%u flags=%u mouse=%d stage=%d\n",sTestFrame,packet.x,packet.y,packet.z,packet.tx,packet.ty,packet.tz,packet.buttons,packet.flags,sMouseMining,sPose.destruction_stage);fclose(trace);}
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
        SteveTerrain_Sync(&sPose);
        for(int i=0;i<128;i++) {
            const SteveCell* cell=&sPose.cells[i];int visible=cell->active;
            if(visible && (cell->flags&1))for(int j=0;j<128;j++)if(i!=j && sPose.cells[j].active && (sPose.cells[j].flags&1) &&
                fabsf(sPose.cells[j].x-cell->x)<0.1f && fabsf(sPose.cells[j].z-cell->z)<0.1f && fabsf(sPose.cells[j].y-(cell->y-1))<0.1f){visible=0;break;}
            if(sCellCollision[i] && (!visible || sCellToken[i]!=cell->token)) {
                if(sCells[i].bgId<BG_ACTOR_MAX)DynaPoly_DeleteBgActor(play,&play->colCtx.dyna,sCells[i].bgId);sCellCollision[i]=0;
            }
            if(visible && !sCellCollision[i]) {
                DynaPolyActor* cube=&sCells[i];memset(cube,0,sizeof(*cube));cube->actor.id=ACTOR_BG_HEAVY_BLOCK;
                cube->actor.update=SteveBlock_NoUpdate;cube->actor.scale.x=cube->actor.scale.y=cube->actor.scale.z=1;
                cube->actor.world.pos={cell->x*20,(cell->y-((cell->flags&1)?1.f:0.f))*20,cell->z*20};
                DynaPolyActor_Init(cube,DPM_UNK);cube->bgId=DynaPoly_SetBgActor(play,&play->colCtx.dyna,&cube->actor,&sBlockCollision);
                sCellCollision[i]=cube->bgId<BG_ACTOR_MAX;sCellToken[i]=cell->token;
            }
        }
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
    if(SteveUi_IsOpen() || sTestMode==1) {memset(&input->cur,0,sizeof(input->cur));memset(&input->press,0,sizeof(input->press));}
    else if(sConnected) {
        if(sPose.item_kind==1 || sPose.item_kind==2 || sPose.item_kind==10) {
            input->cur.button&=~BTN_B;input->press.button&=~BTN_B;
        }
        if(sPose.item_kind==3 || sPose.item_kind==4 || sPose.item_kind==5 || sPose.item_kind==7 || sPose.item_kind==8 || sPose.item_kind>=13) {
            input->cur.button&=~BTN_A;input->press.button&=~BTN_A;
        }
    }
}
void SteveActor_PostUpdate(Actor* actor,PlayState* play) {
    if(sTestMode==2) {
        if(sTestFrame>=270 && sTestFrame<=320){actor->world.pos.x=5;actor->world.pos.z=5;actor->velocity.x=0;actor->velocity.z=0;actor->speedXZ=0;GET_PLAYER(play)->invincibilityTimer=30;}
        const float rise=actor->world.pos.y-sOrigin.y;
        if(sTestFrame<=180 && rise>sJumpTestPeak)sJumpTestPeak=rise;
        if(sTestFrame==180) {
            int placed=0;
            for(int i=0;i<128;i++)if(sPose.cells[i].active && sPose.cells[i].kind==5 &&
                fabsf(sPose.cells[i].x-0.5f)<0.1f && fabsf(sPose.cells[i].z-0.5f)<0.1f)placed=1;
            sJumpTestPlaced=placed;sJumpTestLanded=rise>=19 && (actor->bgCheckFlags&BGCHECKFLAG_GROUND);
        }
        if(sTestFrame==350) {
            FILE* log=fopen("steve-jump-test.log","w");
            if(log){fprintf(log,"%s native mouse controls: jump_peak=%.2f underfoot_block=%d landed=%d reserved=%d glide=%d fireworks_boost=%d left_mine=%d first_person=%d aim=%d\n",
                sJumpTestPeak>22 && sJumpTestPlaced && sJumpTestLanded && sMouseReserved && sMouseGlide && sMouseBoost && sMouseMined && sFirstPersonDraws>20 && SteveUi_TestAimDraws()?"PASS":"FAIL",
                sJumpTestPeak,sJumpTestPlaced,sJumpTestLanded,sMouseReserved,sMouseGlide,sMouseBoost,sMouseMined,sFirstPersonDraws,SteveUi_TestAimDraws());fclose(log);}
            SteveActor_RequestExit();
        }
    }
    if(sTestMode==1 && sTestFrame>=890){actor->world.pos=sOrigin;actor->velocity={0,0,0};actor->speedXZ=0;}
    if(sConnected && sPose.flying) {
        if(!sWasFlying) sGravityBeforeFlight=actor->gravity;
        sWasFlying=1;
        actor->world.pos.x+=sPose.flight_x*20;actor->world.pos.y+=sPose.flight_y*20;actor->world.pos.z+=sPose.flight_z*20;
        actor->velocity.y=0;actor->gravity=0;actor->speedXZ=0;
        Actor_UpdateBgCheckInfo(play,actor,20,15,40,5);
    } else if(sWasFlying) {actor->gravity=sGravityBeforeFlight;sWasFlying=0;}
    if(SteveActor_FirstPerson()){actor->shape.rot.y=(s16)(sLookYaw*32768/3.14159265f);sShadowBeforeFirstPerson=actor->shape.shadowDraw;sShadowMasked=true;actor->shape.shadowDraw=NULL;}
}

static void SteveLoadTexture(PlayState* play,const SteveSkinFace* texture) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
    gDPSetTextureLUT(POLY_OPA_DISP++,G_TT_NONE);
    gDPSetTextureLOD(POLY_OPA_DISP++,G_TL_TILE);
    gSPTexture(POLY_OPA_DISP++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    const int paddedWidth=std::max(8,(texture->width+3)&~3);
    gDPLoadTextureBlock(POLY_OPA_DISP++,(void*)texture->pixels,G_IM_FMT_RGBA,G_IM_SIZ_16b,
        paddedWidth,texture->height,0,G_TX_CLAMP,G_TX_CLAMP,G_TX_NOMASK,G_TX_NOMASK,G_TX_NOLOD,G_TX_NOLOD);
    gDPSetTileSize(POLY_OPA_DISP++,G_TX_RENDERTILE,0,0,(paddedWidth-1)<<G_TEXTURE_IMAGE_FRAC,(texture->height-1)<<G_TEXTURE_IMAGE_FRAC);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void SteveSkinLimb(PlayState* play,int part,float x,float y,float z,float w,float h,float d) {
    int layer,face,i,pass;
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
    for(layer=0;layer<(part>=4?4:3);layer++) {
        int armorPart=part;
        int equipment=part==0?0:part<4?1:layer==2?2:3;
        if(layer>=2 && !(sPose.equipment_flags&(1u<<equipment))) continue;
        if(part>=4 && layer==2) armorPart=part+2;
        Matrix_Push();
        if(layer) {
            const float inflate=layer==1?0.25f:part>=4 && layer==2?0.5f:1.0f;
            Matrix_Scale((w+inflate)/w,(h+inflate)/h,(d+inflate)/d,MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        for(pass=0;pass<(layer>=2 && (sPose.equipment_flags&(1u<<(equipment+4)))?2:1);pass++) {
        if(pass) {
            sArmorGlintDraws++;
            gDPSetBlendColor(POLY_OPA_DISP++,0,0,0,1);
            gDPSetCombineMode(POLY_OPA_DISP++,G_CC_MODULATEIA_PRIM,G_CC_MODULATEIA_PRIM);
            gDPSetPrimColor(POLY_OPA_DISP++,0,0,190,130,255,105);
            gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
        } else {
            gDPSetBlendColor(POLY_OPA_DISP++,0,0,0,128);
            gDPSetCombineMode(POLY_OPA_DISP++,G_CC_DECALRGBA,G_CC_DECALRGBA);
            gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_TEX_EDGE,G_RM_AA_ZB_TEX_EDGE2);
        }
        for(face=0;face<6;face++) {
            const SteveSkinFace* texture=layer<2?&sSkinFaces[part][layer][face]:pass?
                &sArmorGlintFaces[armorPart][(play->gameplayFrames/3)%8][face]:&sArmorFaces[armorPart][face];
            bool opaquePixel=false;const int stride=std::max(8,(texture->width+3)&~3);
            for(int row=0;row<texture->height && !opaquePixel;row++)for(int col=0;col<texture->width;col++)if(texture->pixels[2*(row*stride+col)+1]&1){opaquePixel=true;break;}
            if(!opaquePixel)continue;
            Vtx* vertices=(Vtx*)Graph_Alloc(play->state.gfxCtx,4*sizeof(Vtx));
            memset(vertices,0,4*sizeof(Vtx));
            for(i=0;i<4;i++) {
                int axis;
                for(axis=0;axis<3;axis++) vertices[i].v.ob[axis]=(s16)corners[face][i][axis];
                vertices[i].v.tc[0]=(i==1||i==2)?texture->width*32:0;
                vertices[i].v.tc[1]=(i<2)?texture->height*32:0;
                memset(vertices[i].v.cn,255,4);
            }
            SteveLoadTexture(play,texture);
            gSPVertex(POLY_OPA_DISP++,(uintptr_t)vertices,4,0);
            gSP2Triangles(POLY_OPA_DISP++,0,1,2,0,0,2,3,0);
        }
        }
        Matrix_Pop();
    }
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_DECALRGBA,G_CC_DECALRGBA);
    gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_TEX_EDGE,G_RM_AA_ZB_TEX_EDGE2);
    gDPSetBlendColor(POLY_OPA_DISP++,0,0,0,128);
    gDPSetAlphaCompare(POLY_OPA_DISP++,G_AC_NONE);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
static void SteveSprite(PlayState* play,const SteveSkinFace* texture,float x,float y,float z,float w,float h) {
    int i;
    Vtx* vertices=(Vtx*)Graph_Alloc(play->state.gfxCtx,4*sizeof(Vtx));
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
    SteveLoadTexture(play,texture);
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
static void SteveCuboid(PlayState* play,const SteveSkinFace* faces,float x,float y,float z,float w,float h,float d) {
    Matrix_Push();Matrix_Translate(x,y,z,MTXMODE_APPLY);
    const int order[4]={3,4,5,2};
    for(int side=0;side<4;side++) {
        Matrix_Push();Matrix_RotateY(side*1.5707963f,MTXMODE_APPLY);
        SteveSprite(play,&faces[order[side]],0,0,(side%2)?w:d,(side%2)?d:w,h);Matrix_Pop();
    }
    Matrix_Push();Matrix_RotateX(-1.5707963f,MTXMODE_APPLY);SteveSprite(play,&faces[0],0,0,h,w,d);Matrix_Pop();
    Matrix_Push();Matrix_RotateX(1.5707963f,MTXMODE_APPLY);SteveSprite(play,&faces[1],0,0,h,w,d);Matrix_Pop();
    Matrix_Pop();
}
static void SteveFirstPersonArm(PlayState* play,int part,float centerY){
    Matrix_Push();Matrix_Translate(0,centerY,0,MTXMODE_APPLY);
    SteveCuboid(play,sSkinFaces[part][0],0,0,0,2.25f,6.75f,2.25f);
    SteveCuboid(play,sSkinFaces[part][1],0,0,0,2.5f,7.f,2.5f);
    Matrix_Pop();
}
int SteveHorse_IsRidden(Actor* actor,PlayState* play) {
    Player* player=GET_PLAYER(play);
    return SteveActor_IsReady() && (player->stateFlags1&PLAYER_STATE1_ON_HORSE) && player->rideActor==actor;
}
void SteveHorse_Draw(Actor* actor,PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_TEX_EDGE,G_RM_AA_ZB_TEX_EDGE2);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING|G_CULL_BACK|G_FOG|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    Matrix_Push();Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768.f),MTXMODE_APPLY);Matrix_Scale(2.8f,2.8f,2.8f,MTXMODE_APPLY);
    SteveCuboid(play,sHorseFaces[0],0,17,0,5,5,11);
    Matrix_Push();Matrix_Translate(0,19,9,MTXMODE_APPLY);Matrix_RotateX(-0.52f,MTXMODE_APPLY);
    SteveCuboid(play,sHorseFaces[1],0,5,0,2,6,3.5f);SteveCuboid(play,sHorseFaces[2],0,11,3,3,2.5f,3.5f);
    SteveCuboid(play,sHorseFaces[3],0,10,8,2,2.5f,2.5f);
    SteveCuboid(play,sHorseFaces[5],-2,15,1,1,1.5f,0.5f);SteveCuboid(play,sHorseFaces[5],2,15,1,1,1.5f,0.5f);Matrix_Pop();
    float gait=sinf(play->gameplayFrames*0.4f)*fminf(fabsf(actor->speedXZ)*0.08f,0.8f);
    for(int leg=0;leg<4;leg++) {
        Matrix_Push();Matrix_Translate((leg&1)?4.f:-4.f,12.f,(leg&2)?-8.f:8.f,MTXMODE_APPLY);
        Matrix_RotateX((leg==0||leg==3)?gait:-gait,MTXMODE_APPLY);SteveCuboid(play,sHorseFaces[4],0,-5.5f,0,1.5f,5.5f,1.5f);Matrix_Pop();
    }
    Matrix_Push();Matrix_Translate(0,19,-11,MTXMODE_APPLY);Matrix_RotateX(-0.5f,MTXMODE_APPLY);
    SteveCuboid(play,sHorseFaces[6],0,-6,0,1.5f,7,1);Matrix_Pop();
    Matrix_Pop();CLOSE_DISPS(play->state.gfxCtx);
}
int SteveActor_Draw(Actor* actor, PlayState* play) {
    if (!sConnected || !CVarGetInteger("gEnhancements.SteveBridge",1)) return 0;
    if (!SteveBridge_GetPose(&sPose)) return 0;
    sRenderedFrames++;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Setup 25 uses two cycles. TEXEL0 in its second cycle samples tile 1,
    // which still contains the preceding scene texture. Our meshes load tile 0.
    gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_TEX_EDGE,G_RM_AA_ZB_TEX_EDGE2);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING | G_CULL_BACK | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    if(!SteveActor_FirstPerson()) {
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Player* player=(Player*)actor;
    int riding=(player->stateFlags1&PLAYER_STATE1_ON_HORSE)!=0;
    float seated=riding?1.f:0.f;
    if(play->sceneNum==SCENE_LINKS_HOUSE) {
        if((player->csAction>=38 && player->csAction<=40) || (player->csAction>=60 && player->csAction<=62))seated=1.f;
        else if(player->csAction==41 && player->skelAnime.endFrame>0)seated=1.f-fminf(player->skelAnime.curFrame/player->skelAnime.endFrame,1.f);
    }
    int sitting=seated>0.01f;
    if(riding)Matrix_Translate(0,-13,0,MTXMODE_APPLY);
    Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768.0f),MTXMODE_APPLY);
    if(sPose.flying) {Matrix_Translate(0,15,0,MTXMODE_APPLY);Matrix_RotateX(1.5707963f,MTXMODE_APPLY);}
    Matrix_Scale(1.125f,1.125f,1.125f,MTXMODE_APPLY);
    Matrix_Push(); Matrix_Translate(0,28,0,MTXMODE_APPLY);
    Matrix_RotateX(actor->focus.rot.x*(3.14159265f/32768.0f),MTXMODE_APPLY);
    SteveSkinLimb(play,0,0,0,0,4,4,4); Matrix_Pop();
    SteveSkinLimb(play,1,0,18,0,4,6,2);
    {
        float gait=sitting?0:sinf(play->gameplayFrames*0.3f)*fminf(fabsf(actor->speedXZ)*0.16f,0.8f);
        Matrix_Push();
        Matrix_Translate(-5,22,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.right_arm+gait-0.628f*seated,MTXMODE_APPLY);
        SteveSkinLimb(play,2,-1,-4,0,2,6,2);
        if(sPose.item_kind>0 && sPose.item_kind<16 && sPose.item_kind!=11)
            SteveSprite(play,&sItemFaces[sPose.item_kind],0,-9,3,4,4);
        Matrix_Pop();
        Matrix_Push();
        Matrix_Translate(5,22,0,MTXMODE_APPLY);
        Matrix_RotateX(sPose.left_arm-gait-0.628f*seated,MTXMODE_APPLY);
        SteveSkinLimb(play,3,1,-4,0,2,6,2);
        if (sPose.hand==3) SteveSprite(play,&sShieldFace,0,-6,3,4,6);
        Matrix_Pop();
        Matrix_Push(); Matrix_Translate(-1.9f,12,0,MTXMODE_APPLY);
        Matrix_RotateX(sitting?-1.4137f*seated:-gait,MTXMODE_APPLY);if(sitting)Matrix_RotateY(-0.314f*seated,MTXMODE_APPLY);
        SteveSkinLimb(play,4,0,-6,0,2,6,2); Matrix_Pop();
        Matrix_Push(); Matrix_Translate(1.9f,12,0,MTXMODE_APPLY);
        Matrix_RotateX(sitting?-1.4137f*seated:gait,MTXMODE_APPLY);if(sitting)Matrix_RotateY(0.314f*seated,MTXMODE_APPLY);
        SteveSkinLimb(play,5,0,-6,0,2,6,2); Matrix_Pop();
    }
    if(sPose.chest.id && !(sPose.equipment_flags&2)) {
        float factor=sPose.flying?1.f:0.f;
        float speed=sqrtf(sPose.flight_x*sPose.flight_x+sPose.flight_y*sPose.flight_y+sPose.flight_z*sPose.flight_z);
        if(sPose.flying && sPose.flight_y<0 && speed>0.001f)factor=1.f-powf(-sPose.flight_y/speed,1.5f);
        sWingSpread+=(0.2618f+factor*(1.5708f-0.2618f)-sWingSpread)*0.065f;
        sWingPitch+=(0.2618f+factor*(0.3491f-0.2618f)-sWingPitch)*0.065f;
        float spread=sWingSpread,pitch=sWingPitch;
        Matrix_Push(); Matrix_Translate(5,24,-3,MTXMODE_APPLY);
        Matrix_RotateX(pitch,MTXMODE_APPLY);Matrix_RotateZ(spread,MTXMODE_APPLY);SteveCuboid(play,sWingFaces[0],-5,-10,-1,5,10,1);Matrix_Pop();
        Matrix_Push(); Matrix_Translate(-5,24,-3,MTXMODE_APPLY);
        Matrix_RotateX(pitch,MTXMODE_APPLY);Matrix_RotateZ(-spread,MTXMODE_APPLY);Matrix_Scale(-1,1,1,MTXMODE_APPLY);
        SteveCuboid(play,sWingFaces[0],-5,-10,-1,5,10,1);Matrix_Pop();
    }
    Matrix_Pop();
    } else {
        sFirstPersonDraws++;sFirstPersonPass=true;
        gSPMatrix(POLY_OPA_DISP++,play->view.projectionPtr,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_PROJECTION);
        gSPMatrix(POLY_OPA_DISP++,play->view.viewingPtr,G_MTX_NOPUSH|G_MTX_MUL|G_MTX_PROJECTION);
        gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
        gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING|G_CULL_BOTH|G_FOG|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
        gSPClearGeometryMode(POLY_OPA_DISP++,G_ZBUFFER);
        gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_TEX_EDGE,G_RM_AA_TEX_EDGE2);
        Camera* camera=GET_ACTIVE_CAM(play);
        float bob=sinf(play->gameplayFrames*0.45f)*fminf(fabsf(actor->speedXZ)*0.16f,0.5f);
        float swing=sinf(sPose.right_arm)*3.f;
        if(sHandItemKind!=sPose.item_kind){sEquipProgress=fmaxf(0,sEquipProgress-0.25f);if(sEquipProgress==0)sHandItemKind=sPose.item_kind;}else sEquipProgress=fminf(1,sEquipProgress+0.2f);
        Matrix_Push();Matrix_Translate(camera->eye.x,camera->eye.y,camera->eye.z,MTXMODE_NEW);
        Matrix_RotateY(sLookYaw,MTXMODE_APPLY);Matrix_RotateX(-sLookPitch,MTXMODE_APPLY);
        Matrix_Push();Matrix_Translate(-9+swing,-12+bob-(1-sEquipProgress)*10,22-fabsf(swing),MTXMODE_APPLY);
        Matrix_RotateZ(-0.55f,MTXMODE_APPLY);Matrix_RotateX(-0.65f+sPose.right_arm*0.55f,MTXMODE_APPLY);
        SteveFirstPersonArm(play,2,-3);
        Matrix_Translate(0,2,3,MTXMODE_APPLY);
        if(sHandItemKind==5 || sHandItemKind==13 || sHandItemKind==7){
            SteveSkinFace faces[6];for(auto& face:faces)face=sItemFaces[sHandItemKind];
            if(sHandItemKind==7){faces[0]=sTntTop;faces[1]=sTntBottom;}
            Matrix_RotateY(0.6f,MTXMODE_APPLY);SteveCuboid(play,faces,0,0,0,3,3,3);
        } else if(sHandItemKind>0 && sHandItemKind<16) {
            Matrix_RotateZ(0.55f,MTXMODE_APPLY);SteveSprite(play,&sItemFaces[sHandItemKind],0,2,0,6,6);
        }
        Matrix_Pop();
        Matrix_Push();Matrix_Translate(sPose.hand==3?8.f:12.f,-16+bob+(sPose.hand==3?8:0),23,MTXMODE_APPLY);
        Matrix_RotateY(-0.35f,MTXMODE_APPLY);SteveFirstPersonArm(play,3,-4);
        Matrix_Scale(0.8f,0.8f,0.8f,MTXMODE_APPLY);
        SteveCuboid(play,sShieldModelFaces[0],0,0,-1.5f,6,11,0.5f);
        SteveCuboid(play,sShieldModelFaces[1],0,0,2,1,3,3);Matrix_Pop();Matrix_Pop();
        gSPSetGeometryMode(POLY_OPA_DISP++,G_ZBUFFER);
        gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_TEX_EDGE,G_RM_AA_ZB_TEX_EDGE2);
        sFirstPersonPass=false;
    }
    if (sBlockAlive) {
        SteveTexturedCube(play,sBlock.actor.world.pos.x,sBlock.actor.world.pos.y,sBlock.actor.world.pos.z,
            &sStoneFace,&sStoneFace,&sStoneFace);
    }
    for(int i=0;i<128;i++)if(sPose.cells[i].active) {
        const SteveCell* cell=&sPose.cells[i];
        if(!(cell->flags&1)) {
            const SteveSkinFace* texture=cell->kind==13?&sItemFaces[13]:&sItemFaces[5];
            SteveTexturedCube(play,cell->x*20,cell->y*20,cell->z*20,texture,texture,texture);
        } else {
            if(sCellCollision[i])SteveTexturedCube(play,cell->x*20,(cell->y-1)*20,cell->z*20,&sStoneFace,&sStoneFace,&sStoneFace);
            Matrix_Push();Matrix_Translate(cell->x*20,cell->y*20,cell->z*20,MTXMODE_NEW);
            for(int side=0;side<4;side++) {
                Matrix_Push();Matrix_RotateY(side*1.5707963f,MTXMODE_APPLY);
                float nx=sinf(side*1.5707963f),nz=cosf(side*1.5707963f);
                if(!SteveTerrain_Contains(cell->x*20+nx*20,cell->y*20,cell->z*20+nz*20))SteveSprite(play,&sStoneFace,0,0,10,10,10);
                Matrix_Pop();
            }
            Matrix_Pop();
        }
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
    if (CVarGetInteger("gEnhancements.SteveBridgeDebugOverlay",0)) {
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
} // extern "C"
