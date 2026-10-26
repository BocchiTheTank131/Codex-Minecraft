#include "QualityOfLife.h"
#include "Definitions.h"
#include "Building.h"
#include <algorithm>
#include <tuple>
#include <map>
#include <ctime>

int itemCategory(Item item){
 const auto& d=itemDefinition(item);const auto b=itemToBlock(item);
 if(d.tool!=ToolKind::None||armorSlot(item)>=0||item==Item::Shield)return 3;
 if(isFood(item))return 4;
 if(isPlant(b)||isLeaf(b)||isLog(b)||b==Block::Grass||b==Block::Dirt||b==Block::Sand||b==Block::Snow||b==Block::Water||b==Block::Lava||item==Item::Seeds)return 5;
 if(auto building=buildingDefinition(item)){
  switch(building->shape){
   case BuildingShape::Table:case BuildingShape::Chair:case BuildingShape::Bench:case BuildingShape::Sign:case BuildingShape::Shelf:case BuildingShape::Lantern:case BuildingShape::Chain:case BuildingShape::Candle:case BuildingShape::Campfire:case BuildingShape::Bed:case BuildingShape::Barrel:case BuildingShape::Painting:case BuildingShape::Frame:case BuildingShape::Pot:case BuildingShape::Carpet:return 2;
   default:return 1;
  }
 }
 return b!=Block::Air?1:6;
}
const char* itemCategoryName(int c){static const char* names[]={"ALL","BUILDING","DECORATION","TOOLS / EQUIPMENT","FOOD","PLANTS / NATURE","MISCELLANEOUS","FAVORITES","RECENT"};return c>=0&&c<9?names[c]:names[0];}
void SaveFeedback::complete(bool success,double now){phase=success?Phase::Saved:Phase::Failed;changed=now;if(success){lastSuccess=now;const auto value=std::time(nullptr);std::tm local{};localtime_s(&local,&value);char buffer[32];std::strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&local);time=buffer;}}
std::string SaveFeedback::label(double now,bool pause)const{
 const auto last=lastSuccess>=0?"LAST SUCCESSFUL SAVE: "+time:"NO SUCCESSFUL SAVE THIS SESSION";
 if(phase==Phase::Pending)return pause?"Saving... / "+last:"Saving...";
 if(phase==Phase::Failed&&(pause||now-changed<12))return pause?"SAVE FAILED / "+last:"SAVE FAILED - PREVIOUS CHECKPOINT RETAINED";
 if(pause)return lastSuccess>=0?"LAST SUCCESSFUL SAVE: "+time:"NO SUCCESSFUL SAVE THIS SESSION";
 return phase==Phase::Saved&&now-changed<4?"Saved "+time:"";
}
void Inventory::sortStacks(ItemStack* stacks,int count){
 if(!stacks||count<=0||count> TotalSlots)return;
 std::vector<ItemStack> items;items.reserve(count);
 for(int i=0;i<count;++i)if(!stacks[i].empty()){
  auto moving=stacks[i];
  for(auto& destination:items)if(canMerge(destination,moving)){
   const int n=std::min(moving.count,maxStack(moving.item)-destination.count);destination.count+=n;moving.count-=n;if(moving.count==0)break;
  }
  if(moving.count)items.push_back(moving);
 }
 auto key=[](const ItemStack& s){auto d=buildingDefinition(s.item);return std::make_tuple(itemCategory(s.item),d?int(d->material):int(itemToBlock(s.item)),int(s.item),s.identity,-s.durability,-s.count);};
 std::stable_sort(items.begin(),items.end(),[&](const auto& a,const auto& b){return key(a)<key(b);});
 for(int i=0;i<count;++i)stacks[i]=i<int(items.size())?items[i]:ItemStack{};
}
void Inventory::sortPlayer(int limit){
 limit=std::clamp(limit,0,TotalSlots);std::array<ItemStack,TotalSlots> movable{};int n=0;
 for(int i=0;i<limit;++i)if(!hotbarLocked(i))movable[n++]=slots_[i];
 sortStacks(movable.data(),n);n=0;for(int i=0;i<limit;++i)if(!hotbarLocked(i))slots_[i]=movable[n++];
}
int Inventory::transferContainer(ItemStack* stacks,int count,int action){
 if(!stacks||count<=0||count>27||action<0||action>2)return 0;
 int total=0;
 auto move=[&](ItemStack& source,ItemStack* destination,int size,bool player){
  const int before=source.count;
  for(int pass=0;pass<2;++pass)for(int i=0;i<size&&!source.empty();++i){
   if(player&&hotbarLocked(i))continue;
   auto& target=destination[i];if(pass==0?!canMerge(source,target):!target.empty())continue;
   const int n=std::min(source.count,maxStack(source.item)-(target.empty()?0:target.count));
   if(n<=0)continue;
   if(target.empty()){target=source;target.count=0;}target.count+=n;source.count-=n;if(source.count==0)source.clear();
  }
  total+=before-source.count;
 };
 if(action==2){for(int i=0;i<count;++i)if(!stacks[i].empty())move(stacks[i],slots_.data(),TotalSlots,true);}
 else{
  // Snapshot the container's original types: quick stack must not bootstrap a
  // new item family because another inventory slot was just deposited.
  std::vector<ItemStack> original(stacks,stacks+count);
  for(int i=0;i<TotalSlots;++i)if(!hotbarLocked(i)&&!slots_[i].empty()){
   bool match=action==1;
   for(const auto& s:original)if(s.item==slots_[i].item&&s.identity==slots_[i].identity&&s.durability==slots_[i].durability)match=true;
   if(match)move(slots_[i],stacks,count,false);
  }
 }
 return total;
}

RecipeFeedback recipeFeedback(const Inventory& inv,int index,bool table){
 RecipeFeedback out;const auto& recipes=craftingRecipes();if(index<0||index>=int(recipes.size()))return out;
 const auto& r=recipes[index];out.needsTable=!table&&(r.table||r.w>2||r.h>2);
 // Allocate each physical ingredient once, including items already in the grid.
 std::map<Item,int> stock;for(int i=0;i<Inventory::TotalSlots;++i){auto s=inv.slot(i);if(!s.empty())stock[s.item]+=s.count;}
 for(int i=0;i<Inventory::CraftSlots;++i){auto s=inv.craftSlot(i);if(!s.empty())stock[s.item]+=s.count;}
 std::map<std::string,int> missing;
 for(const auto& ingredient:r.cells)if(!ingredient.empty()){
  bool found=false;for(auto& [item,n]:stock)if(n&&ingredient.accepts(item)){--n;found=true;break;}
  if(!found)++missing[ingredient.group==IngredientGroup::Planks?"ANY PLANKS":ingredient.group==IngredientGroup::Logs?"ANY LOG":itemDefinition(ingredient.item).displayName];
 }
 for(auto& [name,n]:missing)out.missing.push_back(name+" x"+std::to_string(n));
 out.craftable=!out.needsTable&&inv.recipeCraftable(index,table);
 out.message=out.needsTable?"NEEDS 3x3 CRAFTING TABLE":!out.missing.empty()?"MISSING INGREDIENTS":out.craftable?"READY TO CRAFT":"CLEAR GRID / MAKE INVENTORY SPACE";
 return out;
}

bool SignEditor::valid(const std::string& s){if(s.size()>99)return false;int lines=1,width=0;for(unsigned char c:s){if(c=='\n'){if(++lines>4)return false;width=0;}else if(c<32||c>126||++width>24)return false;}return true;}
void SignEditor::begin(const std::string& s){text=valid(s)?s:"";caret=anchor=text.size();}
std::string SignEditor::selected()const{return text.substr(std::min(caret,anchor),std::max(caret,anchor)-std::min(caret,anchor));}
bool SignEditor::insert(const std::string& s){std::string clean;for(unsigned char c:s){if(c=='\r')continue;if(c=='\n'||(c>=32&&c<=126))clean+=char(c);}if(!s.empty()&&clean.empty())return false;auto lo=std::min(caret,anchor),hi=std::max(caret,anchor);auto next=text.substr(0,lo)+clean+text.substr(hi);if(!valid(next))return false;text=std::move(next);caret=anchor=lo+clean.size();return true;}
void SignEditor::erase(bool forward){if(caret!=anchor){insert("");return;}if(forward){if(caret<text.size())anchor=caret+1;}else if(caret)anchor=caret-1;insert("");}
void SignEditor::home(bool end,bool select){auto start=caret?text.rfind('\n',caret-1):std::string::npos;start=start==std::string::npos?0:start+1;auto stop=text.find('\n',caret);caret=end?(stop==std::string::npos?text.size():stop):start;if(!select)anchor=caret;}
void SignEditor::move(int d,bool select,bool line){
 if(!line){if(!select&&caret!=anchor)caret=d<0?std::min(caret,anchor):std::max(caret,anchor);else if(d<0&&caret)--caret;else if(d>0&&caret<text.size())++caret;}
 else{auto start=caret?text.rfind('\n',caret-1):std::string::npos;start=start==std::string::npos?0:start+1;auto col=caret-start;
  if(d<0&&start){auto prev=start>1?text.rfind('\n',start-2):std::string::npos;prev=prev==std::string::npos?0:prev+1;caret=prev+std::min(col,start-1-prev);}
  if(d>0){auto end=text.find('\n',caret);if(end!=std::string::npos){auto next=text.find('\n',end+1);if(next==std::string::npos)next=text.size();caret=end+1+std::min(col,next-end-1);}}
 }
 if(!select)anchor=caret;
}
