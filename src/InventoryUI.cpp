#include "Renderer.h"
#include "Definitions.h"
#include "InventoryTooltip.h"
#include "UiLayout.h"
#include "World.h"
#include "UIManager.h"
#include "QualityOfLife.h"
#include "Settings.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>
namespace {
struct V {
    glm::vec2 p;
    glm::vec4 c;
    glm::vec2 uv;
    float textured;
};
void rect(std::vector<V>& v, float x, float y, float w, float h, glm::vec4 c, int W, int H) {
    float l = x / W * 2 - 1, r = (x + w) / W * 2 - 1, t = 1 - y / H * 2, b = 1 - (y + h) / H * 2;
    v.insert(v.end(),
             {{{l, t}, c, {0, 0}, 0},
              {{l, b}, c, {0, 0}, 0},
              {{r, b}, c, {0, 0}, 0},
              {{l, t}, c, {0, 0}, 0},
              {{r, b}, c, {0, 0}, 0},
              {{r, t}, c, {0, 0}, 0}});
}
void sprite(std::vector<V>& v,
            Item item,
            float x,
            float y,
            float w,
            float h,
            int W,
            int H,
            float alpha = 1) {
    ItemSpriteUv uv;
    if (!itemSpriteUv(item, uv))
        return;
    float l = x / W * 2 - 1, r = (x + w) / W * 2 - 1, t = 1 - y / H * 2, b = 1 - (y + h) / H * 2;
    glm::vec4 c(1, 1, 1, alpha);
    v.insert(v.end(),
             {{{l, t}, c, {uv.u0, uv.v0}, 1},
              {{l, b}, c, {uv.u0, uv.v1}, 1},
              {{r, b}, c, {uv.u1, uv.v1}, 1},
              {{l, t}, c, {uv.u0, uv.v0}, 1},
              {{r, b}, c, {uv.u1, uv.v1}, 1},
              {{r, t}, c, {uv.u1, uv.v0}, 1}});
}
const std::array<unsigned char, 7>& digit(char c) {
    static const std::array<std::array<unsigned char, 7>, 12> d = {{{14, 17, 19, 21, 25, 17, 14},
                                                                    {4, 12, 4, 4, 4, 4, 14},
                                                                    {14, 17, 1, 2, 4, 8, 31},
                                                                    {30, 1, 1, 14, 1, 1, 30},
                                                                    {2, 6, 10, 18, 31, 2, 2},
                                                                    {31, 16, 16, 30, 1, 1, 30},
                                                                    {14, 16, 16, 30, 17, 17, 14},
                                                                    {31, 1, 2, 4, 8, 8, 8},
                                                                    {14, 17, 17, 14, 17, 17, 14},
                                                                    {14, 17, 17, 15, 1, 1, 14},
                                                                    {0, 0, 0, 31, 0, 0, 0},
                                                                    {0, 0, 0, 0, 0, 0, 0}}};
    if (c >= '0' && c <= '9')
        return d[c - '0'];
    if (c == '-')
        return d[10];
    return d[11];
}
void number(std::vector<V>& v,
            const std::string& s,
            float x,
            float y,
            float scale,
            glm::vec4 c,
            int W,
            int H) {
    for (char ch : s) {
        auto& r = digit(ch);
        for (int yy = 0; yy < 7; ++yy)
            for (int xx = 0; xx < 5; ++xx)
                if (r[yy] & (1 << (4 - xx)))
                    rect(v, x + xx * scale, y + yy * scale, scale, scale, c, W, H);
        x += 6 * scale;
    }
}
void text(std::vector<V>& vertices, const std::string& value, float x, float y,
          float scale, glm::vec4 color, int width, int height) {
    for (char raw : value) {
        const auto& rows = uiGlyph(
            static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (rows[row] & (1 << (4 - column)))
                    rect(vertices, x + column * scale, y + row * scale,
                         scale, scale, color, width, height);
            }
        }
        x += 6 * scale;
    }
}
void icon(std::vector<V>& v, const ItemStack& s, float x, float y, float size, int W, int H) {
    if (s.empty())
        return;
    sprite(v, s.item, x + 2, y + 2, size - 4, size - 4, W, H);
    if (s.count > 1) {
        std::string n = std::to_string(s.count);
        number(v,
               n,
               x + size - 4 - static_cast<float>(n.size()) * 6,
               y + size - 11,
               1,
               {1, 1, 1, 1},
               W,
               H);
    }
    int max = Inventory::maxDurability(s.item);
    if (max > 0) {
        float f = std::clamp(static_cast<float>(s.durability) / max, 0.f, 1.f);
        rect(v, x + 5, y + size - 6, size - 10, 3, {.08f, .08f, .08f, 1}, W, H);
        rect(v, x + 5, y + size - 6, (size - 10) * f, 3, {1 - f, f, .08f, 1}, W, H);
    }
}
void slot(std::vector<V>& v,
          const ItemStack& s,
          float x,
          float y,
          float z,
          int W,
          int H,
          bool selected = false) {
    if (selected)
        rect(v, x - 3, y - 3, z + 6, z + 6, {.96f, .96f, .96f, 1}, W, H);
    rect(v, x, y, z, z, {.08f, .08f, .10f, .96f}, W, H);
    rect(v, x + 3, y + 3, z - 6, z - 6, {.30f, .30f, .33f, .96f}, W, H);
    icon(v, s, x, y, z, W, H);
}
void tooltip(std::vector<V>& vertices, UiMode mode, int W, int H,
             const Inventory& inventory, double mouseX, double mouseY,
             const RecipeBookView* book = nullptr, const FurnaceData* furnace = nullptr,
             const ChestData* chest = nullptr,int creativePage=0,const std::vector<Item>* creativeItems=nullptr) {
    const Item item = InventoryTooltip::hoveredItem(mode,W,H,mouseX,mouseY,inventory,book,furnace,chest,creativePage,creativeItems);
    if (item == Item::None) return;
    std::string name = itemDefinition(item).displayName;
    const auto hit=UiLayout::hit(mode,mouseX,mouseY,W,H,creativeCatalog().size());
    const ItemStack* stack=hit.kind==UiSlotKind::EquipmentSlot ? &inventory.equipment(hit.index) :
        hit.kind==UiSlotKind::PlayerInventorySlot ? &inventory.slot(hit.index) : nullptr;
    if(hit.kind==UiSlotKind::CraftingSlot) stack=&inventory.craftSlot(hit.index);
    if(chest && hit.kind==UiSlotKind::ChestSlot) stack=&chest->slots[hit.index];
    if(furnace) {
        if(hit.kind==UiSlotKind::FurnaceInput) stack=&furnace->input;
        if(hit.kind==UiSlotKind::FurnaceFuel) stack=&furnace->fuel;
        if(hit.kind==UiSlotKind::FurnaceOutput) stack=&furnace->output;
    }
    if(item==Item::FilledMap && stack && stack->identity) name+=" #"+std::to_string(stack->identity);
    if(armorDefense(item)>0) name+="  ARMOR "+std::to_string(armorDefense(item));
    if(isDurable(item)) name+="  "+std::to_string(stack?stack->durability:Inventory::maxDurability(item))+"/"+std::to_string(Inventory::maxDurability(item));
    const auto placed=itemToBlock(item);
    const std::string help=item==Item::SurveyorCompass?"RMB: REGIONAL SITE / 10S / 96 USES":item==Item::EchoLantern?"RMB: LIGHT / SNEAK+RMB: CAVE ECHO / 64 USES":item==Item::AncientExplorerTool?"RELIC PICKAXE / IRON TIER / 1024 USES":item==Item::AncientSeal?"REUSABLE V24 VAULT KEY / RARE CRAFTING":item==Item::SurveyorToken?"CRAFT A SURVEYOR COMPASS":item==Item::CavernShard?"CRAFT AN ECHO LANTERN":automationBlock(placed)?automationDescription(placed):"";
    const float scale = std::min(1.5f, (W-24.0f)/std::max(1.0f,name.size()*6.0f));
    UiRect b = InventoryTooltip::bounds(W,H,mouseX,mouseY,std::max(name.size()*6.0f*scale,help.size()*6.f));
    if(!help.empty()){b.height+=15;b.y=std::min(b.y,float(H)-b.height-4);}
    rect(vertices,b.x,b.y,b.width,b.height,{.29f,.23f,.35f,.98f},W,H);
    rect(vertices,b.x+2,b.y+2,b.width-4,b.height-4,{.055f,.045f,.07f,.97f},W,H);
    text(vertices,name,b.x+8,b.y+7,scale,{1,1,1,1},W,H);
    if(!help.empty())text(vertices,help,b.x+8,b.y+22,1.f,{.75f,.85f,.9f,1},W,H);
}
void equipmentSlots(std::vector<V>& vertices,UiMode mode,int W,int H,const Inventory& inv) {
    constexpr const char* labels[]={"H","C","L","B","O"};
    for(int i=0;i<5;++i) {
        const auto b=UiLayout::equipmentSlot(mode,i,W,H);
        slot(vertices,inv.equipment(i),b.x,b.y,b.width,W,H);
        if(inv.equipment(i).empty()) text(vertices,labels[i],b.x+17,b.y+17,1.3f,{.65f,.7f,.75f,1},W,H);
    }
    const auto p=UiLayout::panel(mode,W,H);
    text(vertices,"ARMOR "+std::to_string(inv.armorPoints()),p.x+(mode==UiMode::Creative?360.f:22.f),p.y+(mode==UiMode::Creative?526.f:272.f),1.1f,{.9f,.95f,1,1},W,H);
}
void inventoryActions(std::vector<V>& vertices,UiMode mode,int W,int H,const Inventory& inv){
    for(int i=0;i<9;++i)if(inv.hotbarLocked(i)){const auto b=UiLayout::playerSlot(mode,i,W,H);rect(vertices,b.x,b.y,b.width,3,{1,.75f,.2f,1},W,H);text(vertices,"L",b.x+3,b.y+4,.8f,{1,.8f,.3f,1},W,H);}
    auto button=[&](UiSlotKind kind,const char* label){auto r=UiLayout::inventoryAction(mode,kind,W,H);rect(vertices,r.x,r.y,r.width,r.height,{.17f,.23f,.25f,1},W,H);text(vertices,label,r.x+7,r.y+8,1.05f,{1,1,1,1},W,H);};
    button(UiSlotKind::SortPlayer,mode==UiMode::Creative?"SORT HOTBAR":"SORT PLAYER");
    if(mode==UiMode::Chest){button(UiSlotKind::SortContainer,"SORT CONTENTS");button(UiSlotKind::QuickStack,"QUICK STACK");button(UiSlotKind::DepositAll,"DEPOSIT ALL");button(UiSlotKind::TakeAll,"TAKE ALL");}
    if(mode!=UiMode::Creative){const auto p=UiLayout::panel(mode,W,H);text(vertices,"ALT+CLICK HOTBAR: LOCK",p.x+100,p.y+535,1,{.8f,.85f,.85f,1},W,H);}
}
void flush(std::vector<V>& vertices, GLuint vbo, GLuint vao, GLuint program, GLuint texture,
           GLint atlasUniform) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(V)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(atlasUniform, 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
} // namespace
void Renderer::renderSurvivalUi(
    int W, int H, const Inventory& inv, float hunger, float xp, int level, bool fullbright) const {
    std::vector<V> v;
    float s = 44, g = 4, total = 9 * s + 8 * g, start = (W - total) * .5f,
          y = static_cast<float>(H) - 58.f;
    for (int i = 0; i < 9; ++i) {
        const auto& stack = inv.slot(i);
        int max = Inventory::maxDurability(stack.item);
        if (max > 0) {
            float f = std::clamp(static_cast<float>(stack.durability) / max, 0.f, 1.f);
            rect(v, start + i * (s + g) + 5, y + s - 6, (s - 10) * f, 3, {1 - f, f, .05f, 1}, W, H);
        }
    }
    for (int i = 0; i < 10; ++i) {
        float filled = std::clamp(hunger - i * 2.f, 0.f, 2.f) / 2.f;
        float x = start + total - 14 - i * 18, hy = y - 20;
        rect(v, x, hy, 14, 11, {.13f, .08f, .02f, .9f}, W, H);
        if (filled > 0)
            rect(v, x + 2, hy + 2, 10 * filled, 7, {.92f, .60f, .12f, 1}, W, H);
    }
    rect(v, start, y - 9, total, 5, {.03f, .04f, .03f, .9f}, W, H);
    rect(v,
         start + 2,
         y - 7,
         (total - 4) * std::clamp(xp, 0.f, 1.f),
         2,
         {.28f, .95f, .18f, 1},
         W,
         H);
    number(v, std::to_string(level), W * .5f - 5, y - 23, 1.5f, {.45f, 1, .28f, 1}, W, H);
    const int protection=inv.armorPoints();
    if(protection>0) for(int i=0;i<10;++i) {
        const float x=start+i*18,fill=std::clamp(protection-i*2,0,2)*.5f;
        rect(v,x,y-40,14,12,{.16f,.18f,.22f,.9f},W,H);
        rect(v,x+2,y-38,10*fill,8,{.69f,.8f,.9f,1},W,H);
    }
    if(!inv.equipment(4).empty()) slot(v,inv.equipment(4),start-56,y,44,W,H);
    if (fullbright) {
        rect(v, static_cast<float>(W) - 144.f, 12.f, 132.f, 22.f, {.08f, .08f, .04f, .82f}, W, H);
        number(v, "1", static_cast<float>(W) - 26.f, 16.f, 1.5f, {1, .88f, .2f, 1}, W, H);
    }
    flush(v, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}
void Renderer::renderInventory(
    int W, int H, const Inventory& inv, bool table,
    const RecipeBookView& recipeBook, double mx, double my) const {
    std::vector<V> v;
    v.reserve(8000);
    const UiMode mode = table ? UiMode::CraftingTable : UiMode::Inventory;
    const UiRect panel = UiLayout::panel(mode, W, H);
    const float px = panel.x, py = panel.y;
    rect(v, 0, 0, static_cast<float>(W), static_cast<float>(H), {0, 0, 0, .35f}, W, H);
    rect(v, px, py, panel.width, panel.height, {.08f, .075f, .07f, .97f}, W, H);
    rect(v, px + 10, py + 10, panel.width - 20, panel.height - 20,
         {.34f, .34f, .34f, .98f}, W, H);
    for (int index = 9; index < Inventory::TotalSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(mode, index, W, H);
        slot(v, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H);
    }
    for (int index = 0; index < Inventory::HotbarSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(mode, index, W, H);
        slot(v, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H,
             index == inv.selectedSlot());
    }
    equipmentSlots(v,mode,W,H,inv);
    int n = table ? 3 : 2;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const int index = y * 3 + x;
            const UiRect bounds = UiLayout::craftingSlot(index, W, H);
            slot(v, inv.craftSlot(index), bounds.x, bounds.y, bounds.width, W, H);
        }
    rect(v, px + 278, py + 140, 34, 6, {.72f, .72f, .72f, 1}, W, H);
    rect(v, px + 302, py + 132, 10, 22, {.72f, .72f, .72f, 1}, W, H);
    const UiRect output = UiLayout::craftingOutput(W, H);
    slot(v, inv.craftingOutput(table), output.x, output.y, output.width, W, H);
    number(v, table ? "3" : "2", px + 70, py + 48, 2, {.95f, .9f, .74f, 1}, W, H);
    text(v,"HOVER ITEM + "+recipeBook.usesKey+": RECIPES USING IT",px+70,py+250,1,{.8f,.85f,.85f,1},W,H);
    rect(v, px + 382, py + 34, 245, 245, {.12f, .13f, .14f, .96f}, W, H);
    static constexpr std::array<const char*, 5> categoryNames{
        "ALL", "BLD", "TOOL", "FOOD", "MISC"};
    for (int index = 0; index < 5; ++index) {
        const UiRect bounds = UiLayout::recipeCategory(index, W, H);
        const bool selected = static_cast<int>(recipeBook.category) == index;
        rect(v, bounds.x, bounds.y, bounds.width, bounds.height,
             selected ? glm::vec4(.38f, .48f, .29f, 1)
                      : glm::vec4(.22f, .23f, .25f, 1), W, H);
        text(v, categoryNames[static_cast<std::size_t>(index)],
             bounds.x + 3, bounds.y + 8, .9f, {1, 1, 1, 1}, W, H);
    }
    const UiRect search = UiLayout::recipeSearch(W, H);
    rect(v, search.x, search.y, search.width, search.height,
         recipeBook.searchFocused ? glm::vec4(.33f, .39f, .27f, 1)
                                  : glm::vec4(.21f, .22f, .23f, 1), W, H);
    const auto searchLabel=recipeBook.search.empty()?(recipeBook.ingredientItem==Item::None?std::string("SEARCH"):"USES: "+itemDefinition(recipeBook.ingredientItem).displayName):recipeBook.search;
    text(v, searchLabel.substr(0,28),
         search.x + 7, search.y + 7, 1.25f,
         recipeBook.search.empty() ? glm::vec4(.7f, .7f, .7f, 1)
                                   : glm::vec4(1, 1, 1, 1), W, H);
    const UiRect craftable = UiLayout::recipeCraftable(W, H);
    rect(v, craftable.x, craftable.y, craftable.width, craftable.height,
         recipeBook.craftableOnly ? glm::vec4(.32f, .44f, .24f, 1)
                                  : glm::vec4(.22f, .23f, .25f, 1), W, H);
    text(v, recipeBook.craftableOnly ? "CRAFTABLE ONLY  ON" : "CRAFTABLE ONLY  OFF",
         craftable.x + 7, craftable.y + 7, 1.2f, {1, 1, 1, 1}, W, H);
    const auto& allRecipes = craftingRecipes();
    for (int visible = 0; visible < 8; ++visible) {
        const int entry = recipeBook.page * 8 + visible;
        if (entry >= static_cast<int>(recipeBook.entries.size()))
            break;
        const int recipeIndex = recipeBook.entries[static_cast<std::size_t>(entry)];
        const RecipeInfo& recipe = allRecipes[static_cast<std::size_t>(recipeIndex)];
        const bool available = inv.recipeCraftable(recipeIndex, table);
        const UiRect bounds = UiLayout::recipeItem(visible, W, H);
        const bool hovered = bounds.contains(mx, my);
        rect(v, bounds.x, bounds.y, bounds.width, bounds.height,
             available ? (hovered ? glm::vec4(.40f, .50f, .31f, 1)
                                  : glm::vec4(.26f, .29f, .27f, 1))
                       : glm::vec4(.34f, .19f, .20f, 1), W, H);
        sprite(v, recipe.output, bounds.x + 2, bounds.y + 2,
               bounds.width - 4, bounds.height - 4, W, H, available ? 1.0f : .48f);
        if (recipe.count > 1)
            number(v, std::to_string(recipe.count), bounds.x + 32, bounds.y + 32,
                   1, {1, 1, 1, 1}, W, H);
    }
    for (bool next : {false, true}) {
        const UiRect bounds = UiLayout::recipePageButton(next, W, H);
        rect(v, bounds.x, bounds.y, bounds.width, bounds.height,
             {.24f, .26f, .28f, 1}, W, H);
        text(v, next ? "NEXT" : "PREV", bounds.x + 5, bounds.y + 8,
             1.0f, {1, 1, 1, 1}, W, H);
    }
    text(v, std::to_string(recipeBook.page + 1) + "/" +
                std::to_string(std::max(1, (static_cast<int>(recipeBook.entries.size()) + 7) / 8)),
         px + 475, py + 252, 1.0f, {1, 1, 1, 1}, W, H);
    icon(v, inv.cursorStack(), static_cast<float>(mx) - 22, static_cast<float>(my) - 22, 44, W, H);
    inventoryActions(v,mode,W,H,inv);
    const auto recipeHit=UiLayout::hit(mode,mx,my,W,H,0,recipeBook.entries.size(),recipeBook.page);
    if(recipeHit.kind==UiSlotKind::RecipeItem){const int index=recipeBook.entries[recipeBook.page*8+recipeHit.index];const auto feedback=recipeFeedback(inv,index,table);auto box=InventoryTooltip::bounds(W,H,mx,my,310);const float bh=55+feedback.missing.size()*15;box.y=std::clamp(box.y,4.f,std::max(4.f,H-bh-4));rect(v,box.x,box.y,326,bh,{.05f,.05f,.06f,.98f},W,H);text(v,itemDefinition(craftingRecipes()[index].output).displayName,box.x+8,box.y+8,1.1f,{1,1,1,1},W,H);text(v,feedback.message,box.x+8,box.y+25,1.1f,{1,.8f,.5f,1},W,H);for(int i=0;i<int(feedback.missing.size());++i)text(v,feedback.missing[i],box.x+8,box.y+43+i*15,1.05f,{1,1,1,1},W,H);}
    else tooltip(v,mode,W,H,inv,mx,my,&recipeBook);
    flush(v, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}
void Renderer::renderCreativeInventory(
    int W, int H, const Inventory& inv, double mouseX, double mouseY,int page,
    const std::vector<Item>* creativeItems,const std::string& search,bool searchFocused,int category,const std::vector<int>* favorites) const {
    std::vector<V> vertices;
    vertices.reserve(16000);
    const UiRect panel = UiLayout::panel(UiMode::Creative, W, H);
    const float panelX = panel.x, panelY = panel.y;

    rect(vertices, 0, 0, static_cast<float>(W), static_cast<float>(H), {0, 0, 0, .35f}, W, H);
    rect(vertices, panelX, panelY, panel.width, panel.height,
         {.08f, .075f, .07f, .97f}, W, H);
    rect(vertices, panelX + 10, panelY + 10, panel.width - 20, panel.height - 20,
         {.34f, .34f, .34f, .98f}, W, H);
    const auto& catalog = creativeItems?*creativeItems:creativeCatalog();
    equipmentSlots(vertices,UiMode::Creative,W,H,inv);
    const float catalogHeight = UiLayout::CreativeRows * 48.0f + 16.0f;
    const float catalogWidth = UiLayout::CreativeColumns * 48.0f + 16.0f;
    rect(vertices, panelX + (panel.width - catalogWidth) * .5f, panelY + 25,
         catalogWidth, catalogHeight, {.16f, .16f, .18f, 1}, W, H);
    const char* categories[]={"ALL","BUILD","DECOR","TOOLS","FOOD","NATURE","MISC","FAVS","RECENT"};
    for(int i=0;i<9;++i){auto r=UiLayout::creativeCategory(i,W,H);rect(vertices,r.x,r.y,r.width,r.height,i==category?glm::vec4(.36f,.45f,.26f,1):glm::vec4(.16f,.19f,.20f,1),W,H);text(vertices,categories[i],r.x+5,r.y+6,.9f,{1,1,1,1},W,H);}
    rect(vertices,panelX+450,panelY+475,40,30,{.13f,.15f,.16f,1},W,H);rect(vertices,panelX+550,panelY+475,40,30,{.13f,.15f,.16f,1},W,H);
    text(vertices,"<",panelX+465,panelY+483,1.5f,{1,1,1,1},W,H);text(vertices,">",panelX+565,panelY+483,1.5f,{1,1,1,1},W,H);text(vertices,std::to_string(page+1)+"/"+std::to_string(std::max(1,int((catalog.size()+UiLayout::CreativePageSize-1)/UiLayout::CreativePageSize))),panelX+495,panelY+484,1.2f,{1,1,1,1},W,H);
    const auto searchBox=UiLayout::creativeSearch(W,H);
    rect(vertices,searchBox.x,searchBox.y,searchBox.width,searchBox.height,searchFocused?glm::vec4(.25f,.3f,.32f,1):glm::vec4(.13f,.15f,.16f,1),W,H);
    text(vertices,(search.empty()?std::string("SEARCH ITEMS..."):search)+(searchFocused?"_":""),searchBox.x+8,searchBox.y+9,1.2f,{1,1,1,1},W,H);
    text(vertices,std::to_string(catalog.size()),panelX+385,panelY+484,1.2f,{.8f,.8f,.8f,1},W,H);
    if(catalog.empty())text(vertices,"NO MATCHING ITEMS",panelX+220,panelY+220,1.5f,{.8f,.8f,.8f,1},W,H);
    for (int index = page*UiLayout::CreativePageSize; index < std::min(static_cast<int>(catalog.size()),(page+1)*UiLayout::CreativePageSize); ++index) {
        ItemStack stack{catalog[static_cast<std::size_t>(index)],
                        Inventory::maxStack(catalog[static_cast<std::size_t>(index)]),
                        Inventory::maxDurability(catalog[static_cast<std::size_t>(index)])};
        if (isTool(stack.item))
            stack.count = 1;
        const UiRect bounds = UiLayout::creativeItem(index, W, H);
        slot(vertices, stack, bounds.x, bounds.y, bounds.width, W, H);
        if(favorites&&std::find(favorites->begin(),favorites->end(),int(stack.item))!=favorites->end())text(vertices,"F",bounds.x+3,bounds.y+3,1.2f,{1,.8f,.2f,1},W,H);
    }

    for (int index = 0; index < Inventory::HotbarSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(UiMode::Creative, index, W, H);
        slot(vertices,
             inv.slot(index),
             bounds.x, bounds.y, bounds.width,
             W,
             H,
             index == inv.selectedSlot());
    }

    icon(vertices,
         inv.cursorStack(),
         static_cast<float>(mouseX) - 22.0f,
         static_cast<float>(mouseY) - 22.0f,
         44.0f,
         W,
         H);
    inventoryActions(vertices,UiMode::Creative,W,H,inv);
    text(vertices,"ALT+CLICK HOTBAR: LOCK / ALT+RMB ITEM: FAVORITE",panelX+32,panelY+557,1,{.8f,.85f,.85f,1},W,H);
    tooltip(vertices,UiMode::Creative,W,H,inv,mouseX,mouseY,nullptr,nullptr,nullptr,page,&catalog);
    flush(vertices, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}

void Renderer::renderFurnace(int W,
                             int H,
                             const Inventory& inv,
                             const FurnaceData& furnace,
                             double mouseX,
                             double mouseY) const {
    std::vector<V> vertices;
    vertices.reserve(8000);
    const UiRect panel = UiLayout::panel(UiMode::Furnace, W, H);
    const float panelX = panel.x, panelY = panel.y;
    rect(vertices, 0, 0, static_cast<float>(W), static_cast<float>(H), {0, 0, 0, .35f}, W, H);
    rect(vertices, panelX, panelY, panel.width, panel.height,
         {.08f, .075f, .07f, .97f}, W, H);
    rect(vertices, panelX + 10, panelY + 10, panel.width - 20, panel.height - 20,
         {.34f, .34f, .34f, .98f}, W, H);
    for (int index = 9; index < Inventory::TotalSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(UiMode::Furnace, index, W, H);
        slot(vertices, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H);
    }
    for (int index = 0; index < Inventory::HotbarSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(UiMode::Furnace, index, W, H);
        slot(vertices, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H,
             index == inv.selectedSlot());
    }
    equipmentSlots(vertices,UiMode::Furnace,W,H,inv);
    const UiRect input = UiLayout::furnaceSlot(UiSlotKind::FurnaceInput, W, H);
    const UiRect fuel = UiLayout::furnaceSlot(UiSlotKind::FurnaceFuel, W, H);
    const UiRect output = UiLayout::furnaceSlot(UiSlotKind::FurnaceOutput, W, H);
    slot(vertices, furnace.input, input.x, input.y, input.width, W, H);
    slot(vertices, furnace.fuel, fuel.x, fuel.y, fuel.width, W, H);
    slot(vertices, furnace.output, output.x, output.y, output.width, W, H);
    rect(vertices, panelX + 252, panelY + 148, 88, 10, {.12f, .12f, .12f, 1}, W, H);
    rect(vertices,
         panelX + 254,
         panelY + 150,
         84 * std::clamp(furnace.progress, 0.0f, 1.0f),
         6,
         {.91f, .72f, .22f, 1},
         W,
         H);
    const float burn = furnace.fuelCapacity > 0.0f
                           ? std::clamp(furnace.fuelRemaining / furnace.fuelCapacity, 0.0f, 1.0f)
                           : 0.0f;
    rect(vertices, panelX + 205, panelY + 143, 14, 38, {.12f, .12f, .12f, 1}, W, H);
    rect(vertices,
         panelX + 207,
         panelY + 179 - 34 * burn,
         10,
         34 * burn,
         {1.0f, .36f, .08f, 1},
         W,
         H);
    icon(vertices,
         inv.cursorStack(),
         static_cast<float>(mouseX) - 22,
         static_cast<float>(mouseY) - 22,
         44,
         W,
         H);
    inventoryActions(vertices,UiMode::Furnace,W,H,inv);
    tooltip(vertices,UiMode::Furnace,W,H,inv,mouseX,mouseY,nullptr,&furnace);
    flush(vertices, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}

void Renderer::renderChest(int W,
                           int H,
                           const Inventory& inv,
                           const ChestData& chest,
                           double mouseX,
                           double mouseY) const {
    std::vector<V> vertices;
    vertices.reserve(10000);
    const UiRect panel = UiLayout::panel(UiMode::Chest, W, H);
    const float panelX = panel.x, panelY = panel.y;
    rect(vertices, 0, 0, static_cast<float>(W), static_cast<float>(H), {0, 0, 0, .35f}, W, H);
    rect(vertices, panelX, panelY, panel.width, panel.height,
         {.08f, .075f, .07f, .97f}, W, H);
    rect(vertices, panelX + 10, panelY + 10, panel.width - 20, panel.height - 20,
         {.34f, .34f, .34f, .98f}, W, H);
    equipmentSlots(vertices,UiMode::Chest,W,H,inv);
    for (int index = 0; index < 27; ++index) {
        const UiRect bounds = UiLayout::chestSlot(index, W, H);
        slot(vertices, chest.slots[static_cast<std::size_t>(index)],
             bounds.x, bounds.y, bounds.width, W, H);
    }
    for (int index = 9; index < Inventory::TotalSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(UiMode::Chest, index, W, H);
        slot(vertices, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H);
    }
    for (int index = 0; index < Inventory::HotbarSlots; ++index) {
        const UiRect bounds = UiLayout::playerSlot(UiMode::Chest, index, W, H);
        slot(vertices, inv.slot(index), bounds.x, bounds.y, bounds.width, W, H,
             index == inv.selectedSlot());
    }
    icon(vertices,
         inv.cursorStack(),
         static_cast<float>(mouseX) - 22,
         static_cast<float>(mouseY) - 22,
         44,
         W,
         H);
    inventoryActions(vertices,UiMode::Chest,W,H,inv);
    tooltip(vertices,UiMode::Chest,W,H,inv,mouseX,mouseY,nullptr,nullptr,&chest);
    flush(vertices, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}
void Renderer::renderHeldItem(int W, int H, const ItemStack& stack, float swing, const ItemStack& offhand, bool blocking, float shieldFeedback) const {
    if (stack.empty() && offhand.empty())
        return;
    static thread_local std::vector<V> v;
    v.clear();
    v.reserve(1024); // Bounded held-item/feedback batch, reused on the render thread.
    float wave = std::sin(std::clamp(swing, 0.f, 1.f) * 3.1415926f), size = 168.f,
          x = W - size - 28.f + wave * 28.f, y = H - size + 25.f + wave * 34.f;
    if(!stack.empty()) sprite(v, stack.item, x, y, size, size, W, H, .98f);
    if(!offhand.empty()) {
        const float shieldSize=blocking?225.f:150.f;
        sprite(v,offhand.item,blocking?W*.30f:28.f,H-shieldSize+(blocking?-115.f:22.f),shieldSize,shieldSize,W,H);
        if(shieldFeedback>0) text(v,"BLOCKED",W*.5f-36,H*.65f,1.5f,{.8f,.9f,1,1},W,H);
    }
    flush(v, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}
void Renderer::renderUnderwaterOverlay(int W, int H) const {
    std::vector<V> v;
    rect(v, 0, 0, static_cast<float>(W), static_cast<float>(H), {.02f, .20f, .36f, .28f}, W, H);
    flush(v, uiVbo_, uiVao_, uiProgram_, itemTexture_, uiItemAtlasUniform_);
}
