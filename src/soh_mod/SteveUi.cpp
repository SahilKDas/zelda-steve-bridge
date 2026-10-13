#include "soh_mod/soh_api.h"
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/GuiWindow.h>
#include <fast/Fast3dGui.h>
#include <fast/Fast3dWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/resource/type/Texture.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardKeyToAnyMapping.h>
#include <stb_image.h>
#include <cstring>
#include <memory>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <vector>
extern "C" void SteveActor_SelectSlot(uint32_t slot);
extern "C" void SteveActor_RequestJump(void);
extern "C" void SteveActor_RequestUse(void);
extern "C" void SteveActor_SetMining(int down);
extern "C" void SteveActor_SetUsing(int down);
extern "C" int SteveActor_HudAllowed(void);
extern "C" int SteveActor_FirstPerson(void);
extern "C" void SteveActor_Look(float dx,float dy);
extern "C" int SteveActor_GetAim(int placing,float* x,float* y,float* z);
extern "C" int SteveActor_Project(float x,float y,float z,float aspect,float* u,float* v);
extern "C" int SteveUi_IsOpen(void);
namespace {
int inventoryDraws{},loadedTextures{},crosshairDraws{},previewDraws{};
std::shared_ptr<Fast::Fast3dGui> gameGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}
constexpr const char* names[]={"Empty","Diamond pickaxe","Iron sword","Golden apple","Bow","Cobblestone","Arrow","TNT","Flint and steel","Elytra","Diamond axe","Shield","Diamond chestplate","Obsidian","Firework rocket","Totem of Undying"};
constexpr const char* files[]={"","item/diamond_pickaxe.png","item/iron_sword.png","item/golden_apple.png","item/bow.png","block/cobblestone.png","item/arrow.png","block/tnt_side.png","item/flint_and_steel.png","item/elytra.png","item/diamond_axe.png","","item/diamond_chestplate.png","block/obsidian.png","item/firework_rocket.png","item/totem_of_undying.png"};
void loadHudTexture(const char* name,const char* file){
    int width{},height{},channels{};auto path=std::string("minecraft-assets/gui/sprites/hud/")+file;
    auto* pixels=stbi_load(path.c_str(),&width,&height,&channels,4);if(!pixels)return;
    Fast::Texture texture;texture.Type=Fast::TextureType::RGBA32bpp;texture.Width=static_cast<uint16_t>(width);texture.Height=static_cast<uint16_t>(height);
    texture.ImageDataSize=static_cast<uint32_t>(width*height*4);texture.ImageData=new uint8_t[texture.ImageDataSize];
    std::memcpy(texture.ImageData,pixels,texture.ImageDataSize);stbi_image_free(pixels);gameGui()->LoadGuiTexture(name,texture);
}
void drawAim(const StevePose& pose) {
    crosshairDraws++;
    auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    auto interpreter=window->GetInterpreterWeak().lock();if(!interpreter)return;
    auto rect=interpreter->mGameWindowViewport;const auto* viewport=ImGui::GetMainViewport();
    ImVec2 origin(viewport->Pos.x+rect.x,viewport->Pos.y+rect.y);float width=static_cast<float>(rect.width),height=static_cast<float>(rect.height);
    if(width<1 || height<1)return;
    auto* draw=ImGui::GetForegroundDrawList();ImVec2 center(origin.x+width/2,origin.y+height/2);
    auto crosshair=gameGui()->GetTextureByName("SteveCrosshair");
    if(crosshair)draw->AddImage(crosshair,ImVec2(center.x-12,center.y-12),ImVec2(center.x+12,center.y+12));
    else{draw->AddLine(ImVec2(center.x-6,center.y),ImVec2(center.x+6,center.y),IM_COL32(255,255,255,255));draw->AddLine(ImVec2(center.x,center.y-6),ImVec2(center.x,center.y+6),IM_COL32(255,255,255,255));}
    const int placing=pose.item_kind==5 || pose.item_kind==13 || pose.item_kind==7;
    float x,y,z;if(!SteveActor_GetAim(placing,&x,&y,&z))return;
    previewDraws++;
    ImVec2 corners[8];bool visible[8];
    for(int i=0;i<8;i++){float u=0,v=0;visible[i]=SteveActor_Project(x+((i&1)?10.1f:-10.1f),y+((i&2)?10.1f:-10.1f),z+((i&4)?10.1f:-10.1f),width/height,&u,&v)!=0;corners[i]={origin.x+u*width,origin.y+v*height};}
    draw->PushClipRect(origin,ImVec2(origin.x+width,origin.y+height),true);
    for(int i=0;i<8;i++)for(int axis=1;axis<=4;axis*=2)if(!(i&axis) && visible[i] && visible[i|axis])draw->AddLine(corners[i],corners[i|axis],placing?IM_COL32(90,255,125,220):IM_COL32(245,245,245,220),2);
    draw->PopClipRect();
}
class Inventory final : public Ship::GuiWindow {
    int selected=-1;
    void InitElement() override {
        loadHudTexture("SteveCrosshair","crosshair.png");loadHudTexture("SteveHotbar","hotbar.png");loadHudTexture("SteveHotbarSelection","hotbar_selection.png");
        for(unsigned kind=1;kind<16;kind++) {
            if(!*files[kind]) continue;
            int width{},height{},channels{};
            const auto path=std::string("minecraft-assets/")+files[kind];
            auto* pixels=stbi_load(path.c_str(),&width,&height,&channels,4);
            if(!pixels) continue;
            Fast::Texture texture; texture.Type=Fast::TextureType::RGBA32bpp;
            texture.Width=static_cast<uint16_t>(width); texture.Height=static_cast<uint16_t>(height);
            texture.ImageDataSize=static_cast<uint32_t>(width*height*4);
            texture.ImageData=new uint8_t[texture.ImageDataSize];
            std::memcpy(texture.ImageData,pixels,texture.ImageDataSize); stbi_image_free(pixels);
            gameGui()->LoadGuiTexture("SteveIcon"+std::to_string(kind),texture);
            loadedTextures++;
        }
    }
    void UpdateElement() override {}
    void DrawElement() override {
        inventoryDraws++;
        StevePose pose{};
        if(!SteveBridge_GetPose(&pose)) {ImGui::TextUnformatted("Waiting for Minecraft...");return;}
        ImGui::TextUnformatted("I: close  |  Click two slots to swap  |  J: jump / start elytra flight");
        ImGui::Text("Health %.0f/20   Attack recharge %.0f%%",pose.health,pose.combat_cooldown*100);
        auto slot=[&](int index) {
            ImGui::PushID(index);
            auto item=pose.inventory[index]; auto kind=pose.inventory_kind[index];
            if(kind>15) kind=0;
            const auto position=ImGui::GetCursorScreenPos();
            const bool highlighted=selected==index || (index<10 && pose.selected_slot==static_cast<uint32_t>(index));
            if(highlighted) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.45f,0.38f,0.13f,1));
            if(ImGui::Button("##slot",ImVec2(48,48))) {
                if(selected<0) selected=index;
                else { SteveBridge_RequestCommand(1,static_cast<uint32_t>(selected),static_cast<uint32_t>(index));selected=-1; }
            }
            if(highlighted) ImGui::PopStyleColor();
            auto texture=gameGui()->GetTextureByName("SteveIcon"+std::to_string(kind));
            if(item.count && texture) ImGui::GetWindowDrawList()->AddImage(texture,ImVec2(position.x+6,position.y+3),ImVec2(position.x+42,position.y+39));
            if(item.count) {
                const auto count=(item.metadata&0x80000000u)?std::string("INF"):std::to_string(item.count);
                ImGui::GetWindowDrawList()->AddText(ImVec2(position.x+29,position.y+31),IM_COL32(255,255,255,255),count.c_str());
            }
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();ImGui::TextUnformatted(names[kind]);
                if(item.metadata&0x40000000u)ImGui::TextUnformatted("Unbreakable");else if(item.durability) ImGui::Text("Durability %u",item.durability);
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        };
        ImGui::SeparatorText("Inventory");
        for(int row=0;row<3;row++) for(int col=0;col<9 && 10+row*9+col<36;col++) {if(col)ImGui::SameLine();slot(10+row*9+col);}
        ImGui::SeparatorText("Hotbar");
        for(int col=0;col<10;col++){if(col)ImGui::SameLine();slot(col);}
        ImGui::TextUnformatted("1–9 / 0 or scroll: select hotbar   Offhand: shield (locked)");
        ImGui::SeparatorText("Chest equipment");
        ImGui::Text("%s  durability %u",(pose.equipment_flags&2)?"Enchanted diamond chestplate":pose.chest.id?"Elytra":"Empty",pose.chest.durability);
        ImGui::BeginDisabled(selected<0 || (pose.inventory_kind[selected]!=9 && pose.inventory_kind[selected]!=12 && pose.inventory[selected].count!=0));
        if(ImGui::Button("Equip selected / swap chest")) {SteveBridge_RequestCommand(2,static_cast<uint32_t>(selected),0);selected=-1;}
        ImGui::EndDisabled();
        if(ImGui::Button("Cancel selection")) selected=-1;
    }
public:
    Inventory():GuiWindow("",false,"Steve's inventory",ImVec2(585,410)) {}
};
std::shared_ptr<Inventory> inventory;
class Hotbar final : public Ship::GuiWindow {
    void InitElement() override {}
    void UpdateElement() override {}
    void DrawElement() override {
        StevePose pose{};if(!SteveBridge_GetPose(&pose))return;
        for(unsigned i=0;i<10;i++) {
            if(i)ImGui::SameLine(0,0);
            auto start=ImGui::GetCursorScreenPos();auto end=ImVec2(start.x+40,start.y+44);
            auto* draw=ImGui::GetWindowDrawList();
            auto bar=gameGui()->GetTextureByName("SteveHotbar");
            float left=i==0?0.f:(i==9?162.f:20.f);float right=left+20.f;
            if(bar)draw->AddImage(bar,start,end,ImVec2(left/182,0),ImVec2(right/182,1));else draw->AddRectFilled(start,end,IM_COL32(25,25,25,210));
            if(pose.selected_slot==i){auto selection=gameGui()->GetTextureByName("SteveHotbarSelection");if(selection)draw->AddImage(selection,ImVec2(start.x-4,start.y-2),ImVec2(end.x+4,end.y+2));else draw->AddRect(start,end,IM_COL32(255,225,90,255));}
            auto texture=gameGui()->GetTextureByName("SteveIcon"+std::to_string(pose.inventory_kind[i]));
            if(texture&&pose.inventory[i].count)draw->AddImage(texture,ImVec2(start.x+6,start.y+5),ImVec2(end.x-6,end.y-7));
            auto key=std::to_string((i+1)%10);draw->AddText(ImVec2(start.x+2,start.y+1),IM_COL32(255,255,255,255),key.c_str());
            auto count=(pose.inventory[i].metadata&0x80000000u)?std::string("INF"):std::to_string(pose.inventory[i].count);
            if(pose.inventory[i].count>1)draw->AddText(ImVec2(start.x+21,start.y+30),IM_COL32(255,255,255,255),count.c_str());
            ImGui::Dummy(ImVec2(40,44));
        }
    }
public:
    Hotbar():GuiWindow("",true,"##SteveHotbar") {}
    void Draw() override {
        if(!SteveActor_HudAllowed()||SteveUi_IsOpen())return;
        StevePose aimPose{};if(SteveBridge_GetPose(&aimPose))drawAim(aimPose);
        const auto* viewport=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x+viewport->Size.x/2,viewport->Pos.y+viewport->Size.y-12),ImGuiCond_Always,ImVec2(0.5f,1));
        ImGui::SetNextWindowBgAlpha(0);
        if(ImGui::Begin("##SteveHotbar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings))DrawElement();
        ImGui::End();
    }
};
std::shared_ptr<Hotbar> hotbar;
}
extern "C" int SteveUi_IsOpen() { return inventory && inventory->IsVisible(); }
// Native mouse order is left=0, middle=1, right=2 (different from ImGui).
static bool SteveMouseButton(bool pressed,int button) {
    if(!CVarGetInteger("gEnhancements.SteveBridge",1) || button<0 || button>2)return false;
    const bool usable=SteveActor_HudAllowed() && !SteveUi_IsOpen() && !gameGui()->GetMenuOrMenubarVisible() && !gameGui()->IsMouseOverActivePopup();
    if(button==0)SteveActor_SetMining(pressed && usable);
    if(button==2)SteveActor_SetUsing(pressed && usable);
    if(pressed && usable && button==1)SteveActor_RequestJump();
    if(pressed && usable && button==2)SteveActor_RequestUse();
    return true;
}
extern "C" int SteveUi_TestMouseClick(int button) {
    const bool down=Fast::Fast3dWindow::mouseButtonInterceptor(true,button);
    const bool up=Fast::Fast3dWindow::mouseButtonInterceptor(false,button);
    return down && up && !Ship::Context::GetRawInstance()->GetWindow()->GetMouseState(static_cast<Ship::MouseBtn>(button));
}
extern "C" void SteveUi_Frame(int scancode) {
    Fast::Fast3dWindow::mouseButtonInterceptor=SteveMouseButton;
    if(!inventory) {inventory=std::make_shared<Inventory>();gameGui()->AddGuiWindow(inventory);hotbar=std::make_shared<Hotbar>();gameGui()->AddGuiWindow(hotbar);}
    const bool capture=SteveActor_FirstPerson() && !inventory->IsVisible() && !gameGui()->GetMenuOrMenubarVisible();
    auto window=Ship::Context::GetRawInstance()->GetWindow();window->GetMouseStateManager()->SetAutoCaptureMouse(capture);window->SetMouseCapture(capture);
    if(capture && std::getenv("STEVE_CAPTURE_FIRSTPERSON"))window->SetResolutionMultiplier(1.5f);
    if(capture){auto delta=window->GetMouseDelta();SteveActor_Look(static_cast<float>(delta.x),static_cast<float>(delta.y));}
    else if(inventory->IsVisible() || gameGui()->GetMenuOrMenubarVisible()){SteveActor_SetMining(0);SteveActor_SetUsing(0);}
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_J) && !inventory->IsVisible()) SteveActor_RequestJump();
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_I)) inventory->ToggleVisibility();
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_P))CVarSetInteger("gEnhancements.SteveFirstPerson",!CVarGetInteger("gEnhancements.SteveFirstPerson",1));
    if(scancode>=static_cast<int>(Ship::KbScancode::LUS_KB_1) && scancode<=static_cast<int>(Ship::KbScancode::LUS_KB_0))
        SteveActor_SelectSlot(static_cast<uint32_t>(scancode-static_cast<int>(Ship::KbScancode::LUS_KB_1)));
    if(!inventory->IsVisible()) {
        const auto wheel=Ship::Context::GetRawInstance()->GetWindow()->GetMouseWheel();
        if(wheel.y!=0){StevePose pose{};if(SteveBridge_GetPose(&pose))SteveActor_SelectSlot((pose.selected_slot+(wheel.y>0?9u:1u))%10);}
    }
}
extern "C" void SteveActor_RequestExit(void) { Ship::Context::GetRawInstance()->GetWindow()->Close(); }
extern "C" void SteveUi_TestInventory(int open) {if(inventory){if(open)inventory->Show();else inventory->Hide();}}
extern "C" int SteveUi_TestDraws(void) {return inventoryDraws;}
extern "C" int SteveUi_TextureCount(void) {return loadedTextures;}

extern "C" void SteveUi_CaptureFrame(void) {
    if(!std::getenv("STEVE_CAPTURE_FIRSTPERSON") || !SteveActor_FirstPerson())return;
    static int frames;if(++frames!=240)return;
    auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    auto interpreter=window->GetInterpreterWeak().lock();if(!interpreter || !interpreter->mRendersToFb || interpreter->mGameFb<=0)return;
    auto* api=interpreter->GetCurrentRenderingAPI();if(!api)return;
    const int framebuffer=interpreter->mMsaaLevel>1?interpreter->mGameFbMsaaResolved:interpreter->mGameFb;
    std::vector<uint16_t> pixels(640*480);
    try{api->ReadFramebufferToCPU(framebuffer,640,480,pixels.data());}catch(const std::exception&){return;}
    FILE* output=std::fopen("steve-first-person.ppm","wb");if(!output)return;
    std::fprintf(output,"P6\n640 480\n255\n");
    for(auto pixel:pixels){unsigned char rgb[]={static_cast<unsigned char>(((pixel>>11)&31)*255/31),static_cast<unsigned char>(((pixel>>6)&31)*255/31),static_cast<unsigned char>(((pixel>>1)&31)*255/31)};std::fwrite(rgb,1,3,output);}
    std::fclose(output);
}

extern "C" int SteveUi_TestMouseHeld(int button,int pressed){return Fast::Fast3dWindow::mouseButtonInterceptor(pressed!=0,button);}
extern "C" int SteveUi_TestAimDraws(void){return crosshairDraws>10 && previewDraws>10;}
