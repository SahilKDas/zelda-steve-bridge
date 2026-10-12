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
namespace {
int inventoryDraws{},loadedTextures{};
std::shared_ptr<Fast::Fast3dGui> gameGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}
constexpr const char* names[]={"Empty","Iron pickaxe","Iron sword","Cooked beef","Bow","Block","Arrow","TNT","Flint and steel","Elytra","Netherite axe","Shield"};
constexpr const char* files[]={"","item/iron_pickaxe.png","item/iron_sword.png","item/cooked_beef.png","item/bow.png","block/cobblestone.png","item/arrow.png","block/tnt_side.png","item/flint_and_steel.png","item/elytra.png","item/netherite_axe.png",""};
class Inventory final : public Ship::GuiWindow {
    int selected=-1;
    void InitElement() override {
        for(unsigned kind=1;kind<11;kind++) {
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
        ImGui::TextUnformatted("I: close  |  Click two slots to swap  |  G: glide while airborne");
        ImGui::Text("Health %.0f/20   Attack recharge %.0f%%",pose.health,pose.combat_cooldown*100);
        auto slot=[&](int index) {
            ImGui::PushID(index);
            auto item=pose.inventory[index]; auto kind=pose.inventory_kind[index];
            if(kind>11) kind=0;
            const auto position=ImGui::GetCursorScreenPos();
            const bool highlighted=selected==index || (index<9 && pose.selected_slot==static_cast<uint32_t>(index));
            if(highlighted) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.45f,0.38f,0.13f,1));
            if(ImGui::Button("##slot",ImVec2(48,48))) {
                if(selected<0) selected=index;
                else { SteveBridge_RequestCommand(1,static_cast<uint32_t>(selected),static_cast<uint32_t>(index));selected=-1; }
            }
            if(highlighted) ImGui::PopStyleColor();
            auto texture=gameGui()->GetTextureByName("SteveIcon"+std::to_string(kind));
            if(item.count && texture) ImGui::GetWindowDrawList()->AddImage(texture,ImVec2(position.x+6,position.y+3),ImVec2(position.x+42,position.y+39));
            if(item.count) {
                const auto count=std::to_string(item.count);
                ImGui::GetWindowDrawList()->AddText(ImVec2(position.x+29,position.y+31),IM_COL32(255,255,255,255),count.c_str());
            }
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();ImGui::TextUnformatted(names[kind]);
                if(item.durability) ImGui::Text("Durability %u",item.durability);
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        };
        ImGui::SeparatorText("Inventory");
        for(int row=0;row<3;row++) for(int col=0;col<9;col++) {if(col)ImGui::SameLine();slot(9+row*9+col);}
        ImGui::SeparatorText("Hotbar");
        for(int col=0;col<9;col++){if(col)ImGui::SameLine();slot(col);}
        ImGui::SeparatorText("Chest equipment");
        ImGui::Text("%s  durability %u",pose.chest.id?"Elytra":"Empty",pose.chest.durability);
        ImGui::BeginDisabled(selected<0);
        if(ImGui::Button("Equip selected / swap chest")) {SteveBridge_RequestCommand(2,static_cast<uint32_t>(selected),0);selected=-1;}
        ImGui::EndDisabled();
        if(ImGui::Button("Cancel selection")) selected=-1;
    }
public:
    Inventory():GuiWindow("",false,"Steve's inventory",ImVec2(535,385)) {}
};
std::shared_ptr<Inventory> inventory;
}
extern "C" int SteveUi_IsOpen() { return inventory && inventory->IsVisible(); }
extern "C" void SteveUi_Frame(int scancode) {
    if(!inventory) {inventory=std::make_shared<Inventory>();gameGui()->AddGuiWindow(inventory);}
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_I)) inventory->ToggleVisibility();
    if(scancode==static_cast<int>(Ship::KbScancode::LUS_KB_G) && !inventory->IsVisible()) SteveBridge_RequestCommand(4,0,0);
}
extern "C" void SteveActor_RequestExit(void) { Ship::Context::GetRawInstance()->GetWindow()->Close(); }
extern "C" void SteveUi_TestInventory(int open) {if(inventory){if(open)inventory->Show();else inventory->Hide();}}
extern "C" int SteveUi_TestDraws(void) {return inventoryDraws;}
extern "C" int SteveUi_TextureCount(void) {return loadedTextures;}
