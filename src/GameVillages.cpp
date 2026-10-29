#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Renderer.h"
#include "Sound.h"
#include "UiLayout.h"
#include "VillageUi.h"
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>
bool Game::villageUse(){
 if(!input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT)||input_.actionDown(ControlAction::Sneak)||!interaction_.mobTarget.valid()||!survival_->isVillager(interaction_.mobTarget.index))return false;
 if(!world_->hasLoadedChunkAt(int(std::floor(interaction_.mobTarget.center.x)),int(std::floor(interaction_.mobTarget.center.z)))){chat_.addMessage("Villager terrain is still loading; try again shortly",ChatTone::Warning,glfwGetTime());return true;}
 const auto id=survival_->villagerIdentity(interaction_.mobTarget.index,*world_);if(!id){chat_.addMessage("Village record limit reached; villager retained",ChatTone::Warning,glfwGetTime());return true;}
 villageNpc_=id;villageTab_=villageRow_=0;villageMessage_="Trading uses unlocked inventory slots; equipment and cursor are protected.";villageOpen_=true;journalOpen_=navigationOpen_=false;player_->setBlocking(false);survival_->talkingVillager=id;input_.consumeTypedCharacters();synchronizeCursorCapture();return true;
}
bool Game::villageInput(){if(!villageOpen_)return false;
 auto close=[&]{villageOpen_=false;survival_->talkingVillager=0;input_.consumeTypedCharacters();synchronizeCursorCapture();};
 if(!survival_->villagerAvailable(villageNpc_,player_->position())||player_->isDead()||ui_.state()!=GameState::Playing){close();return false;}
 if(input_.keyPressed(GLFW_KEY_ESCAPE)){close();return true;}
 if(input_.actionPressed(ControlAction::Screenshot))screenshotRequested_=true;
 const int previousTab=villageTab_,previousRow=villageRow_;
 if(input_.keyPressed(GLFW_KEY_LEFT)){villageTab_=(villageTab_+3)%4;villageRow_=0;}
 if(input_.keyPressed(GLFW_KEY_RIGHT)||input_.keyPressed(GLFW_KEY_TAB)){villageTab_=(villageTab_+1)%4;villageRow_=0;}
 const int count=villageTab_==0?5:villageTab_==1?3:2;
 if(input_.keyPressed(GLFW_KEY_UP))villageRow_=(villageRow_+count-1)%count;
 if(input_.keyPressed(GLFW_KEY_DOWN))villageRow_=(villageRow_+1)%count;
 bool activate=input_.keyPressed(GLFW_KEY_ENTER);int action=0;
 if(input_.keyPressed(GLFW_KEY_BACKSPACE))action=2;
 if(input_.keyPressed(GLFW_KEY_R))action=3;
 int width,height;glfwGetFramebufferSize(window_,&width,&height);const float scale=UiLayout::interfaceScale(width,height,settings_.uiScale);width=int(width/scale);height=int(height/scale);
 const auto mouse=input_.framebufferCursorPosition()/double(scale);const VillageUiLayout layout(width,height);
 if(input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)){int tab=layout.tab(mouse.x,mouse.y),row=layout.row(mouse.x,mouse.y,count);if(tab>=0){villageTab_=tab;villageRow_=0;}else if(row>=0){activate=row==villageRow_;villageRow_=row;}}
 if((previousTab!=villageTab_||previousRow!=villageRow_)&&villageTab_==2)villageMessage_=world_->villages.projectSite(villageNpc_,villageRow_,*world_);
 if(activate||action){auto& v=world_->villages;
  if(villageTab_==0&&activate)v.trade(villageNpc_,villageRow_,*inventory_,villageMessage_);
  else if(villageTab_==1){auto n=v.npc(villageNpc_);auto town=n?v.village(n->village):nullptr;if(town)v.request(villageNpc_,villageRow_,action?action:town->requests[villageRow_].status==0?0:1,*inventory_,*player_,villageMessage_);}
  else if(villageTab_==2){if(action==2)v.withdraw(villageNpc_,villageRow_,*inventory_,villageMessage_);else if(activate&&input_.actionDown(ControlAction::Sneak))v.construct(villageNpc_,villageRow_,*world_,*survival_,*player_,villageMessage_);else if(activate)v.contribute(villageNpc_,villageRow_,*inventory_,villageMessage_);villageMessage_+=" / "+v.projectSite(villageNpc_,villageRow_,*world_);}
  sounds_->playClick();
 }
 input_.consumeTypedCharacters();return true;
}
