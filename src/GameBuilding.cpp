#include "Game.h"
#include "Building.h"
#include "Player.h"
#include "Sound.h"
#include "Transportation.h"
#include "WorldTime.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

bool Game::buildingInput(){
 if(!signEditorOpen_)return false;
 auto close=[&]{signEditorOpen_=false;synchronizeCursorCapture();};
 auto d=buildingDefinition(world_->getBlock(signPosition_.x,signPosition_.y,signPosition_.z));
 if(!d || d->shape!=BuildingShape::Sign || glm::length(glm::vec3(signPosition_)-player_->position())>7){close();return true;}
 if(input_.keyPressed(GLFW_KEY_ESCAPE)){close();return true;}
 auto typed=input_.consumeTypedCharacters();for(unsigned char c:typed)if(c>=32&&c<127&&signDraft_.size()<99){auto line=signDraft_.find_last_of('\n');if(signDraft_.size()-(line==std::string::npos?0:line+1)<24)signDraft_+=char(c);}
 if(input_.keyPressed(GLFW_KEY_BACKSPACE)&&!signDraft_.empty())signDraft_.pop_back();
 const bool ctrl=input_.keyDown(GLFW_KEY_LEFT_CONTROL)||input_.keyDown(GLFW_KEY_RIGHT_CONTROL);
 if(ctrl && input_.keyPressed(GLFW_KEY_A))signDraft_.clear();
 if(input_.keyPressed(GLFW_KEY_TAB) && std::count(signDraft_.begin(),signDraft_.end(),'\n')<3)signDraft_+='\n';
 if(input_.keyPressed(GLFW_KEY_ENTER)){if(world_->setBuildingText(signPosition_,signDraft_))chat_.addMessage("Sign saved",ChatTone::Success,glfwGetTime());close();}
 return true;
}

void Game::updateBuilding(float dt){
 if(!world_||!player_||!survival_)return;
 world_->flushBuildingDrops(*survival_);
 for(auto it=pressedButtons_.begin();it!=pressedButtons_.end();){it->second-=dt;if(it->second<=0){auto p=it->first;auto b=world_->getBlock(p.x,p.y,p.z);auto d=buildingDefinition(b);if(d && d->shape==BuildingShape::Button)world_->setBlock(p.x,p.y,p.z,blockState(b,blockProperties(b)&~8));it=pressedButtons_.erase(it);}else ++it;}
 std::optional<glm::ivec3> active;
 if(!spectatorMode_){auto feet=glm::ivec3(glm::floor(player_->position()));for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){auto p=feet+glm::ivec3(x,0,z);auto b=world_->getBlock(p.x,p.y,p.z);auto d=buildingDefinition(b);if(d && d->shape==BuildingShape::PressurePlate){const auto mn=glm::vec3(p),mx=mn+glm::vec3(1,.2f,1);auto a=player_->aabbMinimum(),c=player_->aabbMaximum();if(a.x<mx.x&&c.x>mn.x&&a.z<mx.z&&c.z>mn.z&&a.y<mx.y&&c.y>mn.y)active=p;}}}
 if(pressedPlate_ && pressedPlate_!=active){auto p=*pressedPlate_;auto b=world_->getBlock(p.x,p.y,p.z);auto d=buildingDefinition(b);if(d&&d->shape==BuildingShape::PressurePlate)world_->setBlock(p.x,p.y,p.z,blockState(b,0));}
 if(active && active!=pressedPlate_){auto p=*active;world_->setBlock(p.x,p.y,p.z,blockState(world_->getBlock(p.x,p.y,p.z),8));sounds_->playDoor(true,glm::vec3(p));}pressedPlate_=active;
}

bool Game::buildingUse(Item held,bool offhand){
 if(!interaction_.hasBlockTarget || interaction_.mobTarget.valid())return false;
 const auto target=interaction_.blockTarget.block,normal=interaction_.blockTarget.normal;
 const auto pos=interaction_.blockTarget.adjacent;Block old=world_->getBlock(target.x,target.y,target.z);
 const bool sneak=input_.actionDown(ControlAction::Sneak);
 auto get=[&](glm::ivec3 p){return world_->getBlock(p.x,p.y,p.z);};
 auto set=[&](glm::ivec3 p,Block b){world_->setBlock(p.x,p.y,p.z,b);};
 auto consume=[&]{if(!creativeMode_){if(offhand)inventory_->consumeOffhand();else inventory_->consumeSelected();}};
 auto message=[&](const char* s){chat_.addMessage(s,ChatTone::Normal,glfwGetTime());};
 auto heldStack=[&]{return offhand?inventory_->equipment(4):inventory_->selectedStack();};
 if(auto d=buildingDefinition(old);d && (!sneak || held==Item::None || itemToBlock(held)==Block::Air)) {
  auto p=blockProperties(old);
  if((d->shape==BuildingShape::Bed||d->shape==BuildingShape::Carpet||d->shape==BuildingShape::Candle) && (held==Item::RedFlower||held==Item::YellowFlower||held==Item::BoneMeal||(creativeMode_&&sneak&&held==Item::None))){
   unsigned color=held==Item::None?(((p>>8)+1)&3):held==Item::RedFlower?0:held==Item::YellowFlower?1:2;set(target,blockState(old,(p&~768)|(color<<8)));if(d->shape==BuildingShape::Bed){auto mate=target+((p&16)?-buildingFacing(p):buildingFacing(p));if(buildingIsBed(get(mate)))set(mate,blockState(get(mate),(blockProperties(get(mate))&~768)|(color<<8)));}consume();return true;}
  switch(d->shape){
  case BuildingShape::FenceGate:case BuildingShape::Trapdoor:case BuildingShape::Door:{const auto next=blockState(old,p^8);set(target,next);if(d->shape==BuildingShape::Door){auto mate=target+glm::ivec3(0,p&16?-1:1,0);if(blockType(get(mate))==blockType(old))set(mate,blockState(get(mate),blockProperties(get(mate))^8));}sounds_->playDoor(!(p&8),glm::vec3(target));return true;}
  case BuildingShape::Barrel:ui_.openChest(target);synchronizeCursorCapture();sounds_->playChest(true,glm::vec3(target));return true;
  case BuildingShape::Sign:signPosition_=target;signDraft_=world_->buildingText(target);signEditorOpen_=true;input_.consumeTypedCharacters();synchronizeCursorCapture();return true;
  case BuildingShape::Bed:{auto foot=target;if(p&16)foot-=buildingFacing(p);if(!world_->findBedSpawn(foot)){message("Bed has no safe exit; clear space beside it");return true;}player_->setPersonalBed(foot);
    if(std::sin(WorldTime::angle(timing_.worldTime))<-.05f){timing_.worldTime=WorldTime::DefaultSeconds;message("Slept until morning. Respawn point set.");}else message("Respawn point set. Sleep is available at night.");return true;}
  case BuildingShape::Frame:case BuildingShape::Pot:{auto* chest=world_->chestAt(target,true);if(!chest)return true;auto& owned=chest->slots[0];
   if(sneak && !owned.empty()){auto item=owned;if(survival_->spawnDrop(glm::vec3(target)+.5f,item.item,item.count,item.durability,glm::vec3(.1f,.2f,.1f),0,item.identity))owned.clear();else message("Too many dropped items; displayed item retained");return true;}
   if(!owned.empty()){if(d->shape==BuildingShape::Frame)set(target,blockState(old,(p&~768)|((((p>>8)+1)&3)<<8)));return true;}
   const auto stack=heldStack();bool suitable=!stack.empty();if(d->shape==BuildingShape::Pot){Block plant=itemToBlock(held);suitable=isPlant(plant)||held==Item::Cactus||isLeaf(plant);}
   if(suitable){owned=stack;owned.count=1;if(offhand)inventory_->consumeOffhand();else inventory_->consumeSelected();sounds_->playBlockPlace(old,glm::vec3(target));}return true;}
  case BuildingShape::Button:if(!(p&8) && pressedButtons_.size()<64){set(target,blockState(old,p|8));if(pressedButtons_.size()<64)pressedButtons_.push_back({target,.5f});sounds_->playDoor(true,glm::vec3(target));}return true;
  case BuildingShape::Campfire:case BuildingShape::Candle:set(target,blockState(old,p^8));sounds_->playDoor(!(p&8),glm::vec3(target));return true;
  case BuildingShape::Painting:{auto next=std::uint16_t((p&~768)|((((p>>8)+1)&3)<<8));set(target,blockState(old,next));if(p&32){auto other=target+((p&16)?-buildingFacing((p+1)&3):buildingFacing((p+1)&3));if(blockType(get(other))==Block::Painting)set(other,blockState(get(other),(blockProperties(get(other))&~768)|(next&768)));}return true;}
  default:break;
  }
 }
 if(sneak && (old==Block::Grass||old==Block::Dirt) && itemDefinition(held).tool==ToolKind::Shovel && get(target+glm::ivec3(0,1,0))==Block::Air){set(target,Block::DirtPath);if(!creativeMode_)damageSelectedToolWithSound();return true;}
 if(isLog(old) && itemDefinition(held).tool==ToolKind::Axe){const Block logs[]={Block::Log,Block::BirchLog,Block::SpruceLog,Block::JungleLog,Block::AcaciaLog,Block::DarkOakLog};for(int i=0;i<6;++i)if(blockType(old)==logs[i]){set(target,blockState(Block(int(Block::OakStrippedLog)+i*14),blockProperties(old)));if(!creativeMode_)damageSelectedToolWithSound();return true;}}
 Block b=itemToBlock(held);if(!isBuilding(b))return false;
 const auto hit=player_->cameraPosition()+player_->lookDirection()*interaction_.blockTarget.distance;
 b=buildingPlacement(b,player_->lookDirection(),normal,hit);auto d=buildingDefinition(b);
 if(d->shape==BuildingShape::Slab && blockType(old)==blockType(b) && !(blockProperties(old)&8)){
  const bool top=blockProperties(old)&4;const float h=hit.y-std::floor(hit.y);if((top&&(normal.y<0||normal.y==0&&h<.5f))||(!top&&(normal.y>0||normal.y==0&&h>.5f))){auto boxes=buildingBoxes(blockState(b,8),true);auto a=glm::vec3(target)+boxes.boxes[0].minimum,c=glm::vec3(target)+boxes.boxes[0].maximum,mn=player_->aabbMinimum(),mx=player_->aabbMaximum();if((a.x<mx.x&&c.x>mn.x&&a.y<mx.y&&c.y>mn.y&&a.z<mx.z&&c.z>mn.z)||transportation_->intersectsBox(a,c)||survival_->anyMobIntersects(a,c))return true;set(target,blockState(b,8));consume();return true;}}
 if(pos.y<world_->minimumY()||pos.y>=world_->maximumY() || !world_->collisionReady(glm::vec3(pos),glm::vec3(pos)+glm::vec3(1)) || get(pos)!=Block::Air)return true;
 auto clear=[&](glm::ivec3 where,Block candidate){if(!world_->collisionReady(glm::vec3(where),glm::vec3(where)+glm::vec3(1)) || get(where)!=Block::Air)return false;auto boxes=buildingBoxes(candidate,true);for(int i=0;i<boxes.count;++i){auto a=glm::vec3(where)+boxes.boxes[i].minimum,c=glm::vec3(where)+boxes.boxes[i].maximum;auto mn=player_->aabbMinimum(),mx=player_->aabbMaximum();if(isSolid(candidate)&&((a.x<mx.x&&c.x>mn.x&&a.y<mx.y&&c.y>mn.y&&a.z<mx.z&&c.z>mn.z)||transportation_->intersectsBox(a,c)||survival_->anyMobIntersects(a,c)))return false;}return true;};
 if((d->shape==BuildingShape::Painting||d->shape==BuildingShape::Frame) && normal.y!=0)return true;
 if(!clear(pos,b))return true;
 if(d->shape==BuildingShape::Painting && sneak){b=blockState(b,blockProperties(b)|32);auto other=pos+buildingFacing((blockProperties(b)+1)&3),support=other-buildingFacing(blockProperties(b));auto mate=blockState(b,blockProperties(b)|16);if(!clear(other,mate)||!isSolid(get(support))||!isSolid(get(pos-buildingFacing(blockProperties(b)))))return true;set(pos,b);set(other,mate);consume();sounds_->playBlockPlace(b,glm::vec3(pos));return true;}
 if(buildingNeedsSupport(b)){auto support=pos+glm::ivec3(0,-1,0);if(d->shape==BuildingShape::Chain || (d->shape==BuildingShape::Lantern && (blockProperties(b)&4)))support=pos+glm::ivec3(0,1,0);if(d->shape==BuildingShape::Frame||d->shape==BuildingShape::Painting||d->shape==BuildingShape::Button||(d->shape==BuildingShape::Sign && (blockProperties(b)&16)))support=pos+(d->shape==BuildingShape::Button?buildingFacing(blockProperties(b)):-buildingFacing(blockProperties(b)));if(!isSolid(get(support)) && blockType(get(support))!=Block::Chain)return true;}
 if(d->shape==BuildingShape::Door){const float side=std::abs(player_->lookDirection().x)>std::abs(player_->lookDirection().z)?hit.z-std::floor(hit.z):hit.x-std::floor(hit.x);if(side>.5f)b=blockState(b,blockProperties(b)|128);}
 if(d->shape==BuildingShape::Bed||d->shape==BuildingShape::Door){auto other=pos+(d->shape==BuildingShape::Door?glm::ivec3(0,1,0):buildingFacing(blockProperties(b)));auto mate=blockState(b,blockProperties(b)|16);if(other.y>=world_->maximumY() || !clear(other,mate) || !isSolid(get(pos-glm::ivec3(0,1,0))) || (d->shape==BuildingShape::Bed && !isSolid(get(other-glm::ivec3(0,1,0)))))return true;set(pos,b);set(other,mate);}else set(pos,b);
 consume();sounds_->playBlockPlace(b,glm::vec3(pos));interaction_.heldItemSwing=1;
 if(d->shape==BuildingShape::Sign){signPosition_=pos;signDraft_.clear();signEditorOpen_=true;synchronizeCursorCapture();}
 return true;
}
