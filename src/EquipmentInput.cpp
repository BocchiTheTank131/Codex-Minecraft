#include "Game.h"
#include "Player.h"
#include "World.h"
#include "Survival.h"
#include "Definitions.h"
#include <GLFW/glfw3.h>

void Game::updateEquipmentInput(bool enabled) {
    if(enabled && input_.actionPressed(ControlAction::SwapOffhand)) inventory_->swapOffhand();
    bool raise=enabled && input_.mouseDown(GLFW_MOUSE_BUTTON_RIGHT);
    // Existing primary uses retain priority over the offhand shield.
    // Holding a sword/empty hand leaves RMB available for defense.
    if(raise) {
        const auto held=inventory_->selectedItem();
        if((isFood(held) && player_->hunger()<20) ||
           (interaction_.hasBlockTarget && (itemToBlock(held)!=Block::Air || held==Item::Seeds)))
            raise=false;
        if(input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT) && interaction_.mobTarget.valid() &&
           (held==Item::Wheat || held==Item::Seeds)) raise=false;
        if(interaction_.hasBlockTarget) {
            const auto p=interaction_.blockTarget.block;
            const auto block=world_->getBlock(p.x,p.y,p.z);
            if(itemDefinition(held).tool==ToolKind::Shovel &&
               (block==Block::Grass || block==Block::Dirt) && interaction_.blockTarget.normal.y>0)
                raise=false;
            if(input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT) &&
               (isDoor(block)||block==Block::Chest||block==Block::Furnace||block==Block::CraftingTable))
                raise=false;
        }
    }
    player_->setBlocking(raise || (equipmentPreview_ && smokeTest_.stage==2));
}
