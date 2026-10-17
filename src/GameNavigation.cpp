#include "Game.h"
#include "Transportation.h"
#include "Player.h"
#include "Renderer.h"
#include "Definitions.h"
#include <GLFW/glfw3.h>
#include <sstream>
#include <cmath>

bool Game::navigationInput(){
    if(chat_.isOpen()||ui_.state()!=GameState::Playing)return false;
    if(!navigationOpen_ && input_.keyPressed(GLFW_KEY_N) && std::find(settings_.controls.begin(),settings_.controls.end(),GLFW_KEY_N)==settings_.controls.end()){navigationOpen_=true;input_.consumeTypedCharacters();synchronizeCursorCapture();return true;}
    if(!navigationOpen_){
        if(mapView_ && input_.keyPressed(GLFW_KEY_F6) && std::find(settings_.controls.begin(),settings_.controls.end(),GLFW_KEY_F6)==settings_.controls.end())
            chat_.addMessage(transportation_->refreshMap(viewedMap_)?"Refreshing surveyed pixels from loaded terrain":"Map update busy; refresh again when complete",ChatTone::Normal,glfwGetTime());
        if(input_.keyPressed(GLFW_KEY_M) && std::find(settings_.controls.begin(),settings_.controls.end(),GLFW_KEY_M)==settings_.controls.end()){
            const auto a=inventory_->selectedStack(),b=inventory_->equipment(4);
            const auto id=a.item==Item::FilledMap?a.identity:b.item==Item::FilledMap?b.identity:0;
            if(id){viewedMap_=id;mapView_=!mapView_;}
        }
        return false;
    }
    if(input_.keyPressed(GLFW_KEY_ESCAPE)){waypointDeletePending_=-1;navigationOpen_=false;synchronizeCursorCapture();return true;}
    if(input_.keyPressed(GLFW_KEY_TAB))waypointCoordinatesFocused_=!waypointCoordinatesFocused_;
    auto& text=waypointCoordinatesFocused_?waypointCoordinates_:waypointName_;
    const auto typed=input_.consumeTypedCharacters();for(unsigned char c:typed)if(c>=32&&c<127&&text.size()<(waypointCoordinatesFocused_?64U:24U))text+=char(c);
    if(input_.actionPressed(ControlAction::Screenshot))screenshotRequested_=true;
    const bool control=input_.keyDown(GLFW_KEY_LEFT_CONTROL)||input_.keyDown(GLFW_KEY_RIGHT_CONTROL);
    if(control && input_.keyPressed(GLFW_KEY_A))text.clear();
    if(input_.keyPressed(GLFW_KEY_BACKSPACE)&&!text.empty())text.pop_back();
    const int count=int(transportation_->waypoints().size());
    if(input_.keyPressed(GLFW_KEY_DOWN)&&count)transportation_->selectedWaypoint=(transportation_->selectedWaypoint+1)%count;
    if(input_.keyPressed(GLFW_KEY_UP)&&count)transportation_->selectedWaypoint=(transportation_->selectedWaypoint<0?count-1:(transportation_->selectedWaypoint+count-1)%count);
    if(input_.keyPressed(GLFW_KEY_HOME))transportation_->selectedWaypoint=-1;
    if(waypointDeletePending_!=transportation_->selectedWaypoint || glfwGetTime()>waypointDeleteUntil_)waypointDeletePending_=-1;
    if(input_.keyPressed(GLFW_KEY_DELETE)&&transportation_->selectedWaypoint>=0){
        if(waypointDeletePending_==transportation_->selectedWaypoint){transportation_->removeWaypoint(waypointDeletePending_);waypointDeletePending_=-1;navigationMessage_="Waypoint removed";}
        else {waypointDeletePending_=transportation_->selectedWaypoint;waypointDeleteUntil_=glfwGetTime()+4;navigationMessage_="Remove "+transportation_->waypoints()[waypointDeletePending_].name+"? DELETE again (4s)";}
    }
    if(input_.keyPressed(GLFW_KEY_ENTER)&&control)navigationMessage_=transportation_->renameWaypoint(transportation_->selectedWaypoint,waypointName_)?"Waypoint renamed":"Select a waypoint and enter a name";
    if(input_.keyPressed(GLFW_KEY_ENTER)&&!control){
        glm::vec3 position=player_->position();bool valid=true;
        if(!waypointCoordinates_.empty()){std::istringstream stream(waypointCoordinates_);valid=bool(stream>>position.x>>position.y>>position.z);std::string extra;if(stream>>extra)valid=false;}
        valid=valid&&std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z)&&std::abs(position.x)<30000000&&std::abs(position.z)<30000000&&position.y>=world_->minimumY()&&position.y<world_->maximumY();
        navigationMessage_=!valid?"Invalid coordinates: X/Z +/-30M, Y within this world":transportation_->waypoints().size()>=64?"Waypoint limit reached (64)":transportation_->addWaypoint(waypointName_,position)?"Waypoint saved":"Enter a waypoint name";
    }
    return true;
}
void Game::reclaimMapsIfNeeded(){
    if(!transportation_->atMapLimit())return;
    std::unordered_set<std::uint32_t> refs;
    auto add=[&](const auto& s){if(s.item==Item::FilledMap && s.identity)refs.insert(s.identity);};
    for(int i=0;i<Inventory::TotalSlots;++i)add(inventory_->slot(i));
    for(int i=0;i<Inventory::CraftSlots;++i)add(inventory_->craftSlot(i));
    for(int i=0;i<Inventory::EquipmentSlots;++i)add(inventory_->equipment(i));
    add(inventory_->cursorStack());world_->mapReferences(refs);survival_->mapReferences(refs);
    if(mapView_ && viewedMap_)refs.insert(viewedMap_);
    transportation_->reclaimMaps(refs);
}
bool Game::transportationUse(bool offhand){
    const float reach=std::min(interaction_.hasBlockTarget?std::min(5.f,interaction_.blockTarget.distance):5.f,interaction_.mobTarget.valid()?interaction_.mobTarget.distance:5.f);
    if(transportation_->useBoat(*player_,*world_,player_->cameraPosition(),player_->lookDirection(),reach))return true;
    if(interaction_.hasBlockTarget && !input_.actionDown(ControlAction::Sneak)){const auto cell=interaction_.blockTarget.block;const auto b=world_->getBlock(cell.x,cell.y,cell.z);if(b==Block::Chest||b==Block::Furnace||b==Block::CraftingTable||isDoor(b))return false;}
    const ItemStack held=offhand?inventory_->equipment(4):inventory_->selectedStack();
    if(isBoatItem(held.item)){
        if(transportation_->mounted())return true;
        const auto o=player_->cameraPosition(),d=player_->lookDirection();
        for(float t=.7f;t<=6;t+=.1f){const auto p=o+d*t;const auto cell=glm::ivec3(glm::floor(p));
            if(!world_->hasLoadedChunkAt(cell.x,cell.z))break;
            const auto b=world_->getBlock(cell.x,cell.y,cell.z);
            if(isWater(b)){
                if(glm::distance(glm::vec2(p.x,p.z),glm::vec2(player_->position().x,player_->position().z))>1.1f && transportation_->placeBoat(*world_,held.item,p,std::atan2(d.z,d.x))){if(!creativeMode_){if(offhand)inventory_->consumeOffhand();else inventory_->consumeSelected();}}
                break;
            }
            if(isSolid(b))break;
        }return true;
    }
    if(held.item==Item::EmptyMap || (held.item==Item::FilledMap && !held.identity)){reclaimMapsIfNeeded();const auto id=transportation_->createMap(player_->position());if(id){inventory_->replaceHeld(offhand,held,{Item::FilledMap,1,0,id});viewedMap_=id;mapView_=true;}else chat_.addMessage("128 maps still in use; store fewer map identities to create another",ChatTone::Error,glfwGetTime());return true;}
    if(held.item==Item::FilledMap){
        if(!transportation_->map(held.identity)){chat_.addMessage("Map data unavailable for this world",ChatTone::Error,glfwGetTime());return true;}
        if(input_.actionDown(ControlAction::Sneak)){
            if(glfwGetTime()<mapZoomAfter_)return true;
            if(inventory_->count(Item::Paper)<8){chat_.addMessage("Zooming out needs 8 Paper",ChatTone::Error,glfwGetTime());return true;}
            reclaimMapsIfNeeded();
            const auto id=transportation_->zoomMap(held.identity);if(id && inventory_->replaceHeld(offhand,held,{Item::FilledMap,1,0,id})){inventory_->remove(Item::Paper,8);viewedMap_=id;mapZoomAfter_=glfwGetTime()+.6;chat_.addMessage("Map #"+std::to_string(id)+" zoomed to 1:"+std::to_string(1<<transportation_->map(id)->scale)+" (-8 Paper)",ChatTone::Normal,glfwGetTime());}else chat_.addMessage("Maximum scale or map limit reached",ChatTone::Error,glfwGetTime());
        }else{viewedMap_=held.identity;mapView_=!mapView_;}
        return true;
    }
    if(held.item==Item::Compass){navigationOpen_=true;synchronizeCursorCapture();return true;}
    return false;
}
void Game::updateTransportation(float dt,bool enabled){
    player_->setTransportation(transportation_.get());
    transportation_->update(dt,input_.playerInput(enabled),*world_,*player_,*survival_);
    mapTimer_+=dt;if(mapTimer_<.1f)return;mapTimer_=0;
    const auto a=inventory_->selectedStack(),b=inventory_->equipment(4);
    const auto id=a.item==Item::FilledMap?a.identity:b.item==Item::FilledMap?b.identity:0;
    if(id){viewedMap_=id;transportation_->reveal(*world_,*player_,id);}
    else mapView_=false;
}

void Game::updateTransportationPreview(double now){
    const int stage=int((now-smokeTest_.startTime)/5.0);
    if(stage>=4){glfwSetWindowShouldClose(window_,true);return;}
    if(stage!=smokeTest_.stage){
        smokeTest_.stage=stage;smokeTest_.worldgenScreenshotTaken=false;navigationOpen_=false;
        if(stage==0){
            settings_.renderDistance=8;settings_.terrainLod=0;settings_.vsync=false;settings_.frameLimit=120;glfwSwapInterval(0);world_->setRenderDistance(8);
            showDebug_=false;timing_.worldTime=WorldTime::DefaultSeconds;
            glm::vec3 river(402.5f,69.5f,-2815.5f);
            for(const auto p:{glm::vec3(402.5f,69.5f,-2815.5f),glm::vec3(202.5f,68.5f,-2892.5f),glm::vec3(92.5f,67.5f,-2909.5f),glm::vec3(-1967.5f,64.5f,-2059.5f),glm::vec3(-1148.5f,64.5f,-1997.5f),glm::vec3(-584.5f,65.5f,-2263.5f)}){world_->prepareSpawnTerrain(p);if(isWater(world_->getBlock(int(std::floor(p.x)),int(std::floor(p.y)),int(std::floor(p.z))))){river=p;break;}}
            world_->generate(8,river);
            transportation_=std::make_unique<Transportation>(world_->findSurvivalSpawnNear(0,0));
            for(int i=0;i<6;++i){const auto p=river+glm::vec3((i%3-1)*2.f,0,(i/3)*2.f);world_->prepareSpawnTerrain(p);transportation_->placeBoat(*world_,Item(int(Item::OakBoat)+i),p,.45f+i*.35f);}
            if(transportation_->boats().empty())throw std::runtime_error("Preview needs an unfrozen, loaded V22 river");
            transportation_->addWaypoint("River Camp",river+glm::vec3(120,0,-40));
            player_=std::make_unique<Player>(river+glm::vec3(5,5,8));player_->setCreativeMode(true);player_->setFlying(true);creativeMode_=true;spectatorMode_=false;
            player_->addMouseMovement(-250,-160);inventory_->clear();inventory_->add(Item::Compass);player_->setEquipment(inventory_.get());
        }
        if(stage==1){const auto id=transportation_->createMap(player_->position(),1);inventory_->clear();inventory_->add(Item::FilledMap,1,0,id);inventory_->add(Item::Paper,32);inventory_->add(Item::Compass);inventory_->selectSlot(0);viewedMap_=id;mapView_=true;}
        if(stage==2){navigationOpen_=true;waypointName_="Island Trail";waypointCoordinates_="124 70 -3130";navigationMessage_="Choose a destination or mark your own position";}
        if(stage==3){navigationOpen_=false;mapView_=false;inventory_->clear();for(int i=0;i<6;++i)inventory_->add(Item(int(Item::OakBoat)+i));inventory_->add(Item::EmptyMap);inventory_->add(Item::FilledMap,1,0,viewedMap_);inventory_->add(Item::Compass);ui_.openInventory();}
        synchronizeCursorCapture();
    }
    if(now-smokeTest_.startTime-stage*5>3.5&&!smokeTest_.worldgenScreenshotTaken){screenshotRequested_=true;smokeTest_.worldgenScreenshotTaken=true;}
}
