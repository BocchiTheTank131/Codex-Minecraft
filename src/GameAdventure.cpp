#include "Game.h"
#include "Adventure.h"
#include "Player.h"
#include "Transportation.h"
#include "Renderer.h"
#include "Sound.h"
#include <GLFW/glfw3.h>
#include <cmath>

bool Game::adventureInput(){
    if(chat_.isOpen()||ui_.state()!=GameState::Playing||navigationOpen_||signEditorOpen_)return false;
    if(input_.actionPressed(ControlAction::Journal)){journalOpen_=!journalOpen_;journalPage_=0;input_.consumeTypedCharacters();synchronizeCursorCapture();return true;}
    if(!journalOpen_)return false;
    if(input_.keyPressed(GLFW_KEY_ESCAPE)){journalOpen_=false;synchronizeCursorCapture();return true;}
    if(input_.keyPressed(GLFW_KEY_LEFT)){journalCategory_=(journalCategory_+3)%4;journalPage_=0;}
    if(input_.keyPressed(GLFW_KEY_RIGHT)||input_.keyPressed(GLFW_KEY_TAB)){journalCategory_=(journalCategory_+1)%4;journalPage_=0;}
    if(input_.keyPressed(GLFW_KEY_UP))journalPage_=std::max(0,journalPage_-1);
    if(input_.keyPressed(GLFW_KEY_DOWN)&&!world_->adventure.journal(journalCategory_,journalPage_+1).empty())++journalPage_;
    if(input_.actionPressed(ControlAction::Screenshot))screenshotRequested_=true;
    input_.consumeTypedCharacters();return true;
}
bool Game::adventureUse(bool offhand){
    std::string message;
    const auto held=offhand?inventory_->equipment(4).item:inventory_->selectedItem();
    const bool heldOverride=input_.actionDown(ControlAction::Sneak)&&(itemToBlock(held)!=Block::Air||held==Item::EchoLantern);
    if(interaction_.hasBlockTarget&&world_->generationVersion()>=24&&!heldOverride){
        const auto p=interaction_.blockTarget.block;
        auto plan=world_->cachedUndergroundPlan(adventureFloorDiv(p.x,256),adventureFloorDiv(p.z,256));
        if(plan&&plan->kind!=UndergroundKind::Mineshaft&&!plan->rooms.empty()){
            const auto c=relicChamber(*plan,world_->seed());
            if(p==c.chest()&&blockType(world_->getBlock(p.x,p.y,p.z))==Block::Chest){
                if(world_->adventure.useTreasure(*world_,c,*inventory_,*player_,creativeMode_,message)){
                    if(!message.empty())chat_.addMessage(message,ChatTone::Normal,glfwGetTime());return true;
                }
            }
            if(world_->adventure.useControl(*world_,c,p,message)){
                sounds_->playClick();
                if(!message.empty())chat_.addMessage(message,ChatTone::Normal,glfwGetTime());return true;
            }
        }
    }
    const auto toolItem=offhand?inventory_->equipment(4).item:inventory_->selectedItem();
    const auto toolBefore=offhand?inventory_->equipment(4):inventory_->selectedStack();
    bool newSurvey=false;
    if(world_->adventure.useTool(*world_,*player_,*inventory_,offhand,creativeMode_,message,input_.actionDown(ControlAction::Sneak),&newSurvey)){
        const auto toolAfter=offhand?inventory_->equipment(4):inventory_->selectedStack();
        if(!creativeMode_&&toolBefore.durability>0&&toolAfter.empty())sounds_->playToolBreak();
        if(toolItem==Item::EchoLantern&&message.rfind("Echo:",0)==0)sounds_->playClick();
        else if(toolItem==Item::EchoLantern&&!toolAfter.empty()&&toolAfter.durability<toolBefore.durability)sounds_->playBlockPlace(Block::Torch,player_->position());
        if(!message.empty())chat_.addMessage(message,ChatTone::Normal,glfwGetTime());
        const auto item=toolItem;
        if(newSurvey&&item==Item::SurveyorCompass&&world_->adventure.surveyTarget){
            auto& travel=*transportation_;const auto target=glm::vec3(*world_->adventure.surveyTarget);
            int found=-1;for(int i=0;i<int(travel.waypoints().size());++i)if(travel.waypoints()[i].position==target){found=i;break;}
            if(found<0&&travel.addWaypoint("Surveyor target",target))found=int(travel.waypoints().size())-1;
            if(found>=0)travel.selectedWaypoint=found;
            else chat_.addMessage("Waypoint list full; Surveyor bearing and distance remain available",ChatTone::Normal,glfwGetTime());
        }
        return true;
    }
    return false;
}
