#include "soh_mod/soh_api.h"
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/GuiWindow.h>
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardKeyToAnyMapping.h>
#include <stb_image.h>
#include <cstring>
#include <memory>
#include <string>
#include <cstdlib>
extern "C" void SteveActor_SelectSlot(uint32_t slot);
extern "C" int SteveActor_HudAllowed(void);
extern "C" int SteveUi_IsOpen(void);
namespace {
int inventoryDraws{},loadedTextures{};
std::shared_ptr<Fast::Fast3dGui> gameGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}
constexpr const char* names[]={"Empty","Diamond pickaxe","Iron sword","Golden apple","Bow","Cobblestone","Arrow","TNT","Flint and steel","Elytra","Diamond axe","Shield","Diamond chestplate","Obsidian","Firework rocket","Totem of Undying"};
constexpr const char* files[]={"","item/diamond_pickaxe.png","item/iron_sword.png","item/golden_apple.png","item/bow.png","block/cobblestone.png","item/arrow.png","block/tnt_side.png","item/flint_and_steel.png","item/elytra.png","item/diamond_axe.png","","item/diamond_chestplate.png","block/obsidian.png","item/firework_rocket.png","item/totem_of_undying.png"};
class Inventory final : public Ship::GuiWindow {
    int selected=-1;
    void InitElement() override {
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
        ImGui::TextUnformatted("I: close  |  Click two slots to swap  |  V: glide while airborne");
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
            if(i)ImGui::SameLine(0,3);
            auto start=ImGui::GetCursorScreenPos();auto end=ImVec2(start.x+45,start.y+45);
            auto* draw=ImGui::GetWindowDrawList();
            draw->AddRectFilled(start,end,IM_COL32(25,25,25,210));
            draw->AddRect(start,end,pose.selected_slot==i?IM_COL32(255,225,90,255):IM_COL32(130,130,130,255),0,0,pose.selected_slot==i?2.f:1.f);
            auto texture=gameGui()->GetTextureByName("SteveIcon"+std::to_string(pose.inventory_kind[i]));
            if(texture&&pose.inventory[i].count)draw->AddImage(texture,ImVec2(start.x+6,start.y+5),ImVec2(end.x-6,end.y-7));
            auto key=std::to_string((i+1)%10);draw->AddText(ImVec2(start.x+2,start.y+1),IM_COL32(255,255,255,255),key.c_str());
            auto count=(pose.inventory[i].metadata&0x80000000u)?std::string("INF"):std::to_string(pose.inventory[i].count);
            if(pose.inventory[i].count>1)draw->AddText(ImVec2(start.x+21,start.y+30),IM_COL32(255,255,255,255),count.c_str());
            ImGui::Dummy(ImVec2(45,45));
        }
    }
public:
    Hotbar():GuiWindow("",true,"##SteveHotbar") {}
    void Draw() override {
        if(!SteveActor_HudAllowed()||SteveUi_IsOpen())return;
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
extern "C" void SteveUi_Frame(int scancode) {
    if(!inventory) {inventory=std::make_shared<Inventory>();gameGui()->AddGuiWindow(inventory);hotbar=std::make_shared<Hotbar>();gameGui()->AddGuiWindow(hotbar);}
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_I)) inventory->ToggleVisibility();
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_V) && !inventory->IsVisible()) SteveBridge_RequestCommand(4,0,0);
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
