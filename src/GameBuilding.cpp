#include "Game.h"
#include "Building.h"
#include "Player.h"
#include "Sound.h"
#include "Transportation.h"
#include "WorldTime.h"
#include "Weather.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

bool Game::buildingInput(){
 if(!signEditorOpen_)return false;
 auto close=[&]{signEditorOpen_=false;synchronizeCursorCapture();};
 auto d=buildingDefinition(world_->getBlock(signPosition_.x,signPosition_.y,signPosition_.z));
 if(!d || d->shape!=BuildingShape::Sign || glm::length(glm::vec3(signPosition_)-player_->position())>7){close();return true;}
 if(input_.keyPressed(GLFW_KEY_ESCAPE)){close();return true;}
 if(signEdit_.text!=signDraft_)signEdit_.begin(signDraft_);
 const bool ctrl=input_.keyDown(GLFW_KEY_LEFT_CONTROL)||input_.keyDown(GLFW_KEY_RIGHT_CONTROL);
 const bool shift=input_.keyDown(GLFW_KEY_LEFT_SHIFT)||input_.keyDown(GLFW_KEY_RIGHT_SHIFT);
 auto repeated=[&](int key){const double now=glfwGetTime();if(input_.keyPressed(key)){signRepeatKey_=key;signRepeatAt_=now+.38;return true;}if(signRepeatKey_==key&&input_.keyDown(key)&&now>=signRepeatAt_){signRepeatAt_=now+.045;return true;}return false;};
 if(ctrl&&input_.keyPressed(GLFW_KEY_A)){signEdit_.anchor=0;signEdit_.caret=signEdit_.text.size();}
 if(ctrl&&input_.keyPressed(GLFW_KEY_C)){const auto selection=signEdit_.selected();if(!selection.empty())glfwSetClipboardString(window_,selection.c_str());}
 if(ctrl&&input_.keyPressed(GLFW_KEY_V))if(const char* text=glfwGetClipboardString(window_)){
  std::size_t length=0;while(length<4097&&text[length])++length;
  if(length>4096||!signEdit_.insert(std::string(text,length)))chat_.addMessage("Paste must fit four lines of 24 supported characters",ChatTone::Error,glfwGetTime());
 }
 auto typed=input_.consumeTypedCharacters();if(!ctrl&&!typed.empty())signEdit_.insert(typed);
 if(repeated(GLFW_KEY_BACKSPACE))signEdit_.erase(false);
 if(repeated(GLFW_KEY_DELETE))signEdit_.erase(true);
 for(auto key:{GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_UP,GLFW_KEY_DOWN})if(repeated(key))signEdit_.move(key==GLFW_KEY_LEFT||key==GLFW_KEY_UP?-1:1,shift,key==GLFW_KEY_UP||key==GLFW_KEY_DOWN);
 if(input_.keyPressed(GLFW_KEY_HOME))signEdit_.home(false,shift);
 if(input_.keyPressed(GLFW_KEY_END))signEdit_.home(true,shift);
 if(!ctrl&&input_.keyPressed(GLFW_KEY_ENTER))signEdit_.insert("\n");
 int width,height;glfwGetFramebufferSize(window_,&width,&height);const float scale=UiLayout::interfaceScale(width,height,settings_.uiScale);const auto cursor=input_.framebufferCursorPosition()/double(scale);width=int(width/scale);height=int(height/scale);
 const bool clicked=input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT);
 if(clicked&&UiLayout::signButton(false,width,height).contains(cursor.x,cursor.y)){close();return true;}
 signDraft_=signEdit_.text;
 if((ctrl&&input_.keyPressed(GLFW_KEY_ENTER))||(clicked&&UiLayout::signButton(true,width,height).contains(cursor.x,cursor.y))){if(world_->setBuildingText(signPosition_,signDraft_)){chat_.addMessage("Sign saved",ChatTone::Success,glfwGetTime());close();}}
 return true;
}

void Game::updateBuilding(float dt){
 if(!world_||!player_||!survival_)return;
 world_->flushBuildingDrops(*survival_);
 world_->automation.additionalCollision=[this](const glm::vec3& a,const glm::vec3& b){return transportation_&&transportation_->intersectsBox(a,b);};
 world_->automation.update(dt,*world_,*survival_,player_.get(),glm::smoothstep(-.13f,.17f,std::sin(WorldTime::angle(timing_.worldTime)))*(weather_?weather_->daylightScale():1.f),sounds_.get());
}

std::optional<bool> Game::buildingToolHand() const {
 if(!interaction_.hasBlockTarget || interaction_.mobTarget.valid())return std::nullopt;
 const auto p=interaction_.blockTarget.block;
 const auto block=world_->getBlock(p.x,p.y,p.z);
 auto eligible=[&](const ItemStack& stack){
  if(stack.empty())return false;
  const auto tool=itemDefinition(stack.item).tool;
  if(tool==ToolKind::Shovel)return input_.actionDown(ControlAction::Sneak) &&
      (block==Block::Grass||block==Block::Dirt) && world_->getBlock(p.x,p.y+1,p.z)==Block::Air;
  if(tool==ToolKind::Axe)return blockType(block)==Block::Log || blockType(block)==Block::BirchLog ||
      blockType(block)==Block::SpruceLog || blockType(block)==Block::JungleLog ||
      blockType(block)==Block::AcaciaLog || blockType(block)==Block::DarkOakLog;
  return false;
 };
 // A valid main-hand action wins; otherwise try the offhand for this target.
 if(eligible(inventory_->selectedStack()))return false;
 if(eligible(inventory_->equipment(4)))return true;
 return std::nullopt;
}

PlacementPlan Game::placementPlan(Item item) const {
 PlacementPlan plan;Block b=itemToBlock(item);if(b==Block::Air||!interaction_.hasBlockTarget||interaction_.mobTarget.valid())return plan;
 const auto target=interaction_.blockTarget.block,normal=interaction_.blockTarget.normal;
 auto pos=interaction_.blockTarget.adjacent;const auto hit=player_->cameraPosition()+player_->lookDirection()*(interaction_.blockTarget.distance+(isBuilding(b)?0.f:.001f));
 auto get=[&](glm::ivec3 p){return world_->getBlock(p.x,p.y,p.z);};const auto old=get(target);
 b=buildingPlacement(b,player_->lookDirection(),normal,hit);auto d=buildingDefinition(b);
 if(!d){
  if(isTorch(b)&&normal.y==0)b=blockState(b,(normal.x>0?1:normal.x<0?3:normal.z>0?2:0)+1);
  if(isLog(b))b=blockState(b,normal.x?1:normal.z?2:0);
  if(item==Item::Ladder&&normal.y==0)b=normal.z>0?Block::LadderNorth:normal.z<0?Block::LadderSouth:normal.x>0?Block::LadderWest:Block::LadderEast;
  if(item==Item::Vine&&normal.y==0)b=normal.z>0?Block::VineNorth:normal.z<0?Block::VineSouth:normal.x>0?Block::VineWest:Block::VineEast;
 }
 bool merge=false;
 if(d&&d->shape==BuildingShape::Slab&&blockType(old)==blockType(b)&&!(blockProperties(old)&8)){
  const bool top=blockProperties(old)&4;const float h=hit.y-std::floor(hit.y);
  if((top&&(normal.y<0||(normal.y==0&&h<.5f)))||(!top&&(normal.y>0||(normal.y==0&&h>.5f)))){pos=target;b=blockState(b,8);merge=true;}
 }
 if(!d&&isSlab(b)){
  const float h=hit.y-std::floor(hit.y);const bool halves=(isTopSlab(old)&&(normal.y<0||h<.5f))||(!isTopSlab(old)&&(normal.y>0||h>.5f));
  if(((isWoodenSlab(old)&&item==Item::WoodenSlab)||(isStoneSlab(old)&&item==Item::StoneSlab))&&halves){pos=target;b=item==Item::WoodenSlab?Block::Planks:Block::Stone;merge=true;}
  else if(normal.y<0||(normal.y==0&&h>.5f))b=isWoodenSlab(b)?Block::WoodenSlabTop:Block::StoneSlabTop;
 }
 if(d&&d->shape==BuildingShape::Door){const float side=std::abs(player_->lookDirection().x)>std::abs(player_->lookDirection().z)?hit.z-std::floor(hit.z):hit.x-std::floor(hit.x);if(side>.5f)b=blockState(b,blockProperties(b)|128);}
 if(d)b=buildingConnectedState(*world_,pos,b);
 if(!d&&b==Block::WoodenDoor){
  const auto look=player_->lookDirection();const int facing=std::abs(look.x)>std::abs(look.z)?(look.x>0?2:3):(look.z>0?1:0);
  constexpr int lx[]={-1,1,0,0},lz[]={0,0,-1,1};const glm::ivec3 left(lx[facing],0,lz[facing]),right=-left;
  auto support=[&](glm::ivec3 side){int n=0;for(int y=0;y<2;++y){auto block=get(pos+side+glm::ivec3(0,y,0));n+=isSolid(block)&&!isDoor(block);}return n;};
  auto neighbor=[&](glm::ivec3 side){auto block=get(pos+side);return isDoor(block)&&!isDoorUpper(block)&&doorFacing(block)==facing;};
  const auto local=hit-(glm::vec3(target)+glm::vec3(.5));bool hinge=neighbor(left)!=neighbor(right)?neighbor(left):support(left)!=support(right)?support(right)>support(left):local.x*left.x+local.z*left.z<0;
  b=doorBlock(facing,false,hinge,false);plan.blocks.push_back({pos+glm::ivec3(0,1,0),doorBlock(facing,true,hinge,false)});
 }
 plan.blocks.push_back({pos,b});
 if(d&&(d->shape==BuildingShape::Bed||d->shape==BuildingShape::Door)){
  const auto other=pos+(d->shape==BuildingShape::Door?glm::ivec3(0,1,0):buildingFacing(blockProperties(b)));plan.blocks.push_back({other,blockState(b,blockProperties(b)|16)});
 }
 if(d&&d->shape==BuildingShape::Painting&&input_.actionDown(ControlAction::Sneak)){
  b=blockState(b,blockProperties(b)|32);plan.blocks[0].block=b;const auto other=pos+buildingFacing((blockProperties(b)+1)&3);plan.blocks.push_back({other,blockState(b,blockProperties(b)|16)});
 }
 auto fail=[&](const char* reason){plan.reason=reason;return plan;};
 std::vector<std::pair<glm::ivec3,Block>> registrations;for(auto part:plan.blocks)registrations.push_back({part.position,part.block});
 if(!world_->automation.canRegisterPlan(registrations))return fail("AUTOMATION CAPACITY REACHED");
 for(auto part:plan.blocks){
  const auto p=part.position;const auto candidate=part.block;
  if(!world_->automation.canRegister(p,candidate))return fail("AUTOMATION CAPACITY REACHED");
  if(p.y<world_->minimumY()||p.y>=world_->maximumY())return fail("WORLD HEIGHT LIMIT");
  if(!world_->collisionReady(glm::vec3(p),glm::vec3(p)+glm::vec3(1)))return fail("TERRAIN LOADING");
  if(!(merge&&p==target)&&get(p)!=Block::Air&&(d||!isWater(get(p))))return fail("SPACE OCCUPIED");
  auto boxes=buildingBoxes(candidate,true);
  if(!d){const auto g=blockGeometry(candidate);boxes.add(g.minX,g.minY,g.minZ,g.maxX,g.maxY,g.maxZ);}
  for(int i=0;i<boxes.count&&isSolid(candidate);++i){auto a=glm::vec3(p)+boxes.boxes[i].minimum,c=glm::vec3(p)+boxes.boxes[i].maximum,mn=player_->aabbMinimum(),mx=player_->aabbMaximum();
   if(a.x<mx.x&&c.x>mn.x&&a.y<mx.y&&c.y>mn.y&&a.z<mx.z&&c.z>mn.z)return fail("PLAYER IN THE WAY");
   if(transportation_->intersectsBox(a,c)||(d&&survival_->anyMobIntersects(a,c)))return fail("ENTITY IN THE WAY");
  }
  if(!d&&((item==Item::Ladder||item==Item::Vine)&&normal.y!=0))return fail("NEEDS WALL SUPPORT");
  if(!d&&(candidate==Block::SugarCane||isVine(candidate))&&!world_->canPlacePlant(p,candidate))return fail("NEEDS SUPPORT");
  if(d&&(d->shape==BuildingShape::Painting||d->shape==BuildingShape::Frame)&&normal.y!=0)return fail("NEEDS WALL SUPPORT");
  if(d&&buildingNeedsSupport(candidate)){
   auto support=p-glm::ivec3(0,1,0);const auto props=blockProperties(candidate);
   if(d->shape==BuildingShape::Chain||(d->shape==BuildingShape::Lantern&&(props&4)))support=p+glm::ivec3(0,1,0);
   if(d->shape==BuildingShape::Frame||d->shape==BuildingShape::Painting||d->shape==BuildingShape::Button||(d->shape==BuildingShape::Sign&&(props&16)))support=p+(d->shape==BuildingShape::Button?buildingFacing(props):-buildingFacing(props));
   bool pendingSupport=false;for(auto other:plan.blocks)if(other.position==support&&isSolid(other.block))pendingSupport=true;
   if(!pendingSupport&&!isSolid(get(support))&&blockType(get(support))!=Block::Chain)return fail("NEEDS SUPPORT");
  }
  if(d&&(d->shape==BuildingShape::Bed||d->shape==BuildingShape::Door)&&!isSolid(get(p-glm::ivec3(0,1,0)))){
   bool lower=false;for(auto other:plan.blocks)if(other.position==p-glm::ivec3(0,1,0))lower=true;if(!lower)return fail("NEEDS SUPPORT");
  }
 }
 plan.valid=true;return plan;
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
  if(automationBlock(old)&&blockType(old)!=Block::Hopper&&world_->automation.interact(*world_,target)){sounds_->playClick();return true;}
  switch(d->shape){
  case BuildingShape::Hopper:ui_.openChest(target);synchronizeCursorCapture();return true;
  case BuildingShape::FenceGate:case BuildingShape::Trapdoor:case BuildingShape::Door:{const auto next=blockState(old,p^8);set(target,next);if(d->shape==BuildingShape::Door){auto mate=target+glm::ivec3(0,p&16?-1:1,0);if(blockType(get(mate))==blockType(old))set(mate,blockState(get(mate),blockProperties(get(mate))^8));}sounds_->playDoor(!(p&8),glm::vec3(target));return true;}
  case BuildingShape::Barrel:ui_.openChest(target);synchronizeCursorCapture();sounds_->playChest(true,glm::vec3(target));return true;
  case BuildingShape::Sign:signPosition_=target;signDraft_=world_->buildingText(target);signEditorOpen_=true;signEdit_.begin(signDraft_);input_.consumeTypedCharacters();synchronizeCursorCapture();return true;
  case BuildingShape::Bed:{auto foot=target;if(p&16)foot-=buildingFacing(p);if(!world_->findBedSpawn(foot)){message("Bed has no safe exit; clear space beside it");return true;}player_->setPersonalBed(foot);
    if(std::sin(WorldTime::angle(timing_.worldTime))<-.05f){timing_.worldTime=WorldTime::DefaultSeconds;message("Slept until morning. Respawn point set.");}else message("Respawn point set. Sleep is available at night.");return true;}
  case BuildingShape::Frame:case BuildingShape::Pot:{auto* chest=world_->chestAt(target,true);if(!chest)return true;auto& owned=chest->slots[0];
   if(sneak && !owned.empty()){auto item=owned;if(survival_->spawnDrop(glm::vec3(target)+.5f,item.item,item.count,item.durability,glm::vec3(.1f,.2f,.1f),0,item.identity))owned.clear();else message("Too many dropped items; displayed item retained");return true;}
   if(!owned.empty()){if(d->shape==BuildingShape::Frame)set(target,blockState(old,(p&~768)|((((p>>8)+1)&3)<<8)));return true;}
   const auto stack=heldStack();bool suitable=!stack.empty();if(d->shape==BuildingShape::Pot){Block plant=itemToBlock(held);suitable=isPlant(plant)||held==Item::Cactus||isLeaf(plant);}
   if(suitable){owned=stack;owned.count=1;if(offhand)inventory_->consumeOffhand();else inventory_->consumeSelected();sounds_->playBlockPlace(old,glm::vec3(target));}return true;}
  case BuildingShape::Button:world_->automation.interact(*world_,target);sounds_->playClick();return true;
  case BuildingShape::Campfire:case BuildingShape::Candle:set(target,blockState(old,p^8));sounds_->playDoor(!(p&8),glm::vec3(target));return true;
  case BuildingShape::Painting:{auto next=std::uint16_t((p&~768)|((((p>>8)+1)&3)<<8));set(target,blockState(old,next));if(p&32){auto other=target+((p&16)?-buildingFacing((p+1)&3):buildingFacing((p+1)&3));if(blockType(get(other))==Block::Painting)set(other,blockState(get(other),(blockProperties(get(other))&~768)|(next&768)));}return true;}
  default:break;
  }
 }
 if(sneak && (old==Block::Grass||old==Block::Dirt) && itemDefinition(held).tool==ToolKind::Shovel && get(target+glm::ivec3(0,1,0))==Block::Air){set(target,Block::DirtPath);if(!creativeMode_)damageToolWithSound(offhand);return true;}
 if(isLog(old) && itemDefinition(held).tool==ToolKind::Axe){const Block logs[]={Block::Log,Block::BirchLog,Block::SpruceLog,Block::JungleLog,Block::AcaciaLog,Block::DarkOakLog};for(int i=0;i<6;++i)if(blockType(old)==logs[i]){set(target,blockState(Block(int(Block::OakStrippedLog)+i*14),blockProperties(old)));if(!creativeMode_)damageToolWithSound(offhand);return true;}}
 Block b=itemToBlock(held);if(!isBuilding(b))return false;
 const auto plan=placementPlan(held);if(!plan.valid)return true;
 std::vector<std::pair<glm::ivec3,Block>> before,after;
 for(const auto& part:plan.blocks){before.push_back({part.position,get(part.position)});after.push_back({part.position,part.block});}
 if(!world_->automationBatch(before,after))return true;
 consume();sounds_->playBlockPlace(b,glm::vec3(pos));interaction_.heldItemSwing=1;
 if(creativeMode_)ui_.rememberItem(held);
 if(buildingDefinition(b)->shape==BuildingShape::Sign){signPosition_=plan.blocks.front().position;signDraft_.clear();signEdit_.begin(signDraft_);signEditorOpen_=true;synchronizeCursorCapture();}
 return true;
}

std::string Game::interactionHint()const {
 if(!world_||!player_||player_->isDead()||spectatorMode_||!interaction_.hasBlockTarget||interaction_.mobTarget.valid()||signEditorOpen_)return "";
 const auto p=interaction_.blockTarget.block;const auto b=world_->getBlock(p.x,p.y,p.z);
 const bool off=buildingToolHand().value_or(inventory_->selectedStack().empty()||(!inventory_->equipment(4).empty()&&!hasMainHandUse(inventory_->selectedItem())));
 const auto held=off?inventory_->equipment(4).item:inventory_->selectedItem();
 const auto sneak=controlKeyName(settings_.controls[int(ControlAction::Sneak)]);
 const bool sneaking=input_.actionDown(ControlAction::Sneak);
 if(automationBlock(b)&&(!sneaking||held==Item::None||itemToBlock(held)==Block::Air)){
  if(blockType(b)==Block::Hopper)return "RMB: OPEN HOPPER";
  if(blockType(b)==Block::Lever)return "RMB: TOGGLE POWER";
  if(blockType(b)==Block::Repeater)return "RMB: DELAY "+std::to_string(1+((blockProperties(b)>>4)&3))+" TICKS";
  if(blockType(b)==Block::AutoHarvester)return std::string("RMB: REPLANT ")+(blockProperties(b)&64?"ON":"OFF");
  return "SIGNAL "+std::to_string(automationSignal(b));
 }
 if(auto d=buildingDefinition(b);d&&(!sneaking||held==Item::None||itemToBlock(held)==Block::Air)){
  if((d->shape==BuildingShape::Bed||d->shape==BuildingShape::Carpet||d->shape==BuildingShape::Candle)&&(held==Item::RedFlower||held==Item::YellowFlower||held==Item::BoneMeal))return "RMB: DYE";
  switch(d->shape){
   case BuildingShape::FenceGate:return "RMB: "+std::string(blockProperties(b)&8?"CLOSE GATE":"OPEN GATE");
   case BuildingShape::Trapdoor:case BuildingShape::Door:return "RMB: "+std::string(blockProperties(b)&8?"CLOSE":"OPEN");
   case BuildingShape::Bed:{auto foot=p;if(blockProperties(b)&16)foot-=buildingFacing(blockProperties(b));if(!world_->findBedSpawn(foot))return "";return std::sin(WorldTime::angle(timing_.worldTime))<-.05f?"RMB: SLEEP":"RMB: SET RESPAWN";}
   case BuildingShape::Sign:return "RMB: EDIT SIGN";
   case BuildingShape::Barrel:return "RMB: OPEN BARREL";
   case BuildingShape::Frame:case BuildingShape::Pot:{const auto* storage=world_->chestAt(p,false);if(storage&&!storage->slots[0].empty())return sneak+" + RMB: REMOVE ITEM";if(held==Item::None)return "";if(d->shape==BuildingShape::Pot&&!isPlant(itemToBlock(held))&&held!=Item::Cactus&&!isLeaf(itemToBlock(held)))return "";return "RMB: DISPLAY ITEM";}
   case BuildingShape::Candle:case BuildingShape::Campfire:return "RMB: TOGGLE FLAME";
   case BuildingShape::Button:return blockProperties(b)&8?"":"RMB: PRESS BUTTON";
   default:break;
  }
 }
 if(!sneaking||itemToBlock(held)==Block::Air){if(b==Block::Chest)return "RMB: OPEN CHEST";if(b==Block::Furnace)return "RMB: OPEN FURNACE";if(b==Block::CraftingTable)return "RMB: CRAFT";}
 if(buildingToolHand().has_value()&&itemDefinition(held).tool==ToolKind::Axe)return "RMB: STRIP LOG";
 if((b==Block::Dirt||b==Block::Grass)&&world_->getBlock(p.x,p.y+1,p.z)==Block::Air)
  for(auto item:{inventory_->selectedItem(),inventory_->equipment(4).item})if(itemDefinition(item).tool==ToolKind::Shovel)return sneak+" + RMB: CREATE PATH";
 return "";
}
