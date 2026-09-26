#include "Survival.h"

#include "Player.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>

namespace {
constexpr char InventoryMagic[8] = {'V','X','I','N','V','4','\0','\0'};

bool isPickaxe(Item i) { return i==Item::WoodPickaxe || i==Item::StonePickaxe || i==Item::IronPickaxe; }
bool isAxe(Item i) { return i==Item::WoodAxe || i==Item::StoneAxe || i==Item::IronAxe; }
bool isShovel(Item i) { return i==Item::WoodShovel || i==Item::StoneShovel || i==Item::IronShovel; }
int tier(Item i) {
    if (i==Item::IronPickaxe || i==Item::IronAxe || i==Item::IronShovel) return 3;
    if (i==Item::StonePickaxe || i==Item::StoneAxe || i==Item::StoneShovel) return 2;
    if (i==Item::WoodPickaxe || i==Item::WoodAxe || i==Item::WoodShovel) return 1;
    return 0;
}
template<class T> bool writeValue(std::ofstream& f, const T& v) { f.write(reinterpret_cast<const char*>(&v), sizeof(v)); return !!f; }
template<class T> bool readValue(std::ifstream& f, T& v) { f.read(reinterpret_cast<char*>(&v), sizeof(v)); return !!f; }
bool same(Item item, const ItemStack& s) { return !s.empty() && s.item==item; }
bool empty(const ItemStack& s) { return s.empty(); }

bool lineOfSightTarget(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& center,
                       float reach, float radius, float& along) {
    glm::vec3 d = glm::normalize(direction);
    glm::vec3 delta = center-origin;
    along = glm::dot(delta,d);
    if (along<0.0f || along>reach) return false;
    return glm::length(delta-d*along)<=radius;
}
}

Inventory::Inventory() {
    add(Item::Grass, 12);
    add(Item::Log, 4);
    add(Item::Torch, 6);
    add(Item::WoodPickaxe, 1);
    add(Item::Seeds, 6);
    add(Item::Bread, 2);
}

void Inventory::selectSlot(int slot) { selectedSlot_=std::clamp(slot,0,HotbarSlots-1); }
void Inventory::cycleSlot(int direction) {
    selectedSlot_=(selectedSlot_+direction)%HotbarSlots;
    if (selectedSlot_<0) selectedSlot_+=HotbarSlots;
}
Item Inventory::selectedItem() const { return selectedStack().empty()?Item::None:selectedStack().item; }
const ItemStack& Inventory::selectedStack() const { return slots_[selectedSlot_]; }
const ItemStack& Inventory::slot(int i) const { static ItemStack none; return i>=0&&i<TotalSlots?slots_[i]:none; }
const ItemStack& Inventory::craftSlot(int i) const { static ItemStack none; return i>=0&&i<CraftSlots?crafting_[i]:none; }
int Inventory::count(Item item) const { int n=0; for (const auto& s:slots_) if(s.item==item)n+=s.count; return n; }

int Inventory::maxStack(Item item) { return isTool(item)?1:(item==Item::None?0:64); }
int Inventory::maxDurability(Item item) {
    if (tier(item)==1) return 60;
    if (tier(item)==2) return 132;
    if (tier(item)==3) return 251;
    return 0;
}
void Inventory::normalize(ItemStack& s) {
    if (s.item==Item::None || s.count<=0) { s.clear(); return; }
    s.count=std::min(s.count,maxStack(s.item));
    int maxD=maxDurability(s.item);
    if (maxD>0) s.durability=std::clamp(s.durability,1,maxD); else s.durability=0;
}
bool Inventory::canMerge(const ItemStack& a,const ItemStack& b) {
    return !a.empty()&&!b.empty()&&a.item==b.item&&!isTool(a.item);
}
int Inventory::add(Item item,int amount,int durability) {
    if(item==Item::None||amount<=0)return amount;
    const int max=maxStack(item);
    if (!isTool(item)) for(auto& s:slots_) if(s.item==item&&s.count<max) {int n=std::min(amount,max-s.count);s.count+=n;amount-=n;if(!amount)return 0;}
    for(auto& s:slots_) if(s.empty()) {int n=std::min(amount,max);s={item,n,isTool(item)?(durability<0?maxDurability(item):durability):0};normalize(s);amount-=n;if(!amount)return 0;}
    return amount;
}
bool Inventory::remove(Item item,int amount) {
    if(count(item)<amount)return false;
    for(auto& s:slots_) if(s.item==item&&amount>0){int n=std::min(amount,s.count);s.count-=n;amount-=n;normalize(s);} return true;
}
bool Inventory::consumeSelected(int amount) { auto& s=slots_[selectedSlot_];if(s.count<amount)return false;s.count-=amount;normalize(s);return true; }
bool Inventory::damageSelectedTool(int amount) {
    auto& s=slots_[selectedSlot_]; if(!isTool(s.item))return false;
    s.durability-=amount; if(s.durability<=0)s.clear(); return true;
}

void Inventory::clickStack(ItemStack& slot,ItemStack& cursor,bool rightClick) {
    normalize(slot);normalize(cursor);
    if(!rightClick){
        if(cursor.empty()){cursor=slot;slot.clear();}
        else if(slot.empty()){slot=cursor;cursor.clear();}
        else if(canMerge(slot,cursor)){int n=std::min(cursor.count,maxStack(slot.item)-slot.count);slot.count+=n;cursor.count-=n;normalize(cursor);}
        else std::swap(slot,cursor);
    } else {
        if(cursor.empty()&&!slot.empty()){int n=(slot.count+1)/2;cursor=slot;cursor.count=n;slot.count-=n;normalize(slot);}
        else if(!cursor.empty()&&slot.empty()){slot=cursor;slot.count=1;--cursor.count;normalize(cursor);}
        else if(canMerge(slot,cursor)&&slot.count<maxStack(slot.item)){++slot.count;--cursor.count;normalize(cursor);}
    }
}

ItemStack Inventory::craftingOutput(bool table) const {
    auto at=[&](int x,int y)->const ItemStack&{return crafting_[y*3+x];};
    auto active=[&](int i){return table||i==0||i==1||i==3||i==4;};
    int countUsed=0;for(int i=0;i<9;++i)if(active(i)&&!crafting_[i].empty())++countUsed;
    if(countUsed==1)for(int i=0;i<9;++i)if(active(i)&&same(Item::Log,crafting_[i]))return {Item::Planks,4,0};
    if(countUsed==2){
        int width=table?3:2,height=table?3:2;
        for(int x=0;x<width;++x)for(int y=0;y+1<height;++y)if(same(Item::Planks,at(x,y))&&same(Item::Planks,at(x,y+1)))return {Item::Stick,4,0};
        bool coal=false,stick=false;for(int i=0;i<9;++i)if(active(i)){coal|=same(Item::CoalOre,crafting_[i]);stick|=same(Item::Stick,crafting_[i]);}
        if(coal&&stick)return {Item::Torch,4,0};
    }
    if(countUsed==4&&same(Item::Planks,at(0,0))&&same(Item::Planks,at(1,0))&&same(Item::Planks,at(0,1))&&same(Item::Planks,at(1,1)))return {Item::CraftingTable,1,0};
    if(table){
        for(int y=0;y<3;++y)if(countUsed==3&&same(Item::Wheat,at(0,y))&&same(Item::Wheat,at(1,y))&&same(Item::Wheat,at(2,y)))return {Item::Bread,1,0};
        for(Item m:{Item::Planks,Item::Stone,Item::IronOre})if(countUsed==5&&same(m,at(0,0))&&same(m,at(1,0))&&same(m,at(2,0))&&same(Item::Stick,at(1,1))&&same(Item::Stick,at(1,2))){Item out=m==Item::Planks?Item::WoodPickaxe:(m==Item::Stone?Item::StonePickaxe:Item::IronPickaxe);return {out,1,maxDurability(out)};}
        for(Item m:{Item::Planks,Item::Stone,Item::IronOre}){bool a=countUsed==5&&same(m,at(0,0))&&same(m,at(1,0))&&same(m,at(0,1))&&same(Item::Stick,at(1,1))&&same(Item::Stick,at(1,2));bool mirror=countUsed==5&&same(m,at(1,0))&&same(m,at(2,0))&&same(m,at(2,1))&&same(Item::Stick,at(1,1))&&same(Item::Stick,at(1,2));if(a||mirror){Item out=m==Item::Planks?Item::WoodAxe:(m==Item::Stone?Item::StoneAxe:Item::IronAxe);return {out,1,maxDurability(out)};}}
        for(Item m:{Item::Planks,Item::Stone,Item::IronOre})for(int x=0;x<3;++x)if(countUsed==3&&same(m,at(x,0))&&same(Item::Stick,at(x,1))&&same(Item::Stick,at(x,2))){Item out=m==Item::Planks?Item::WoodShovel:(m==Item::Stone?Item::StoneShovel:Item::IronShovel);return {out,1,maxDurability(out)};}
    }
    return {};
}bool Inventory::consumeCraftPattern(bool table,const ItemStack& output){if(output.empty())return false;for(int i=0;i<9;++i)if((table||i==0||i==1||i==3||i==4)&&!crafting_[i].empty()){--crafting_[i].count;normalize(crafting_[i]);}return true;}
bool Inventory::takeCraftingOutput(bool table){
    ItemStack out=craftingOutput(table);if(out.empty())return false;
    if(cursor_.empty())cursor_=out;else if(canMerge(cursor_,out)&&cursor_.count+out.count<=maxStack(out.item))cursor_.count+=out.count;else return false;
    return consumeCraftPattern(table,out);
}

bool Inventory::handleInventoryClick(double mx,double my,int w,int h,bool right,bool table,bool shift){
    const float px=w*0.5f-325,py=h*0.5f-280,s=44,g=4,start=px+100;
    auto inside=[&](float x,float y){return mx>=x&&mx<x+s&&my>=y&&my<y+s;};
    auto quickMove=[&](int source,int first,int last){
        ItemStack moving=slots_[source];if(moving.empty())return true;
        if(!isTool(moving.item))for(int i=first;i<last&&moving.count>0;++i)if(canMerge(slots_[i],moving)){
            int n=std::min(moving.count,maxStack(moving.item)-slots_[i].count);slots_[i].count+=n;moving.count-=n;
        }
        for(int i=first;i<last&&moving.count>0;++i)if(slots_[i].empty()){
            int n=std::min(moving.count,maxStack(moving.item));slots_[i]=moving;slots_[i].count=n;moving.count-=n;
        }
        slots_[source]=moving;normalize(slots_[source]);return true;
    };
    if(inside(px+330,py+125))return takeCraftingOutput(table);
    int n=table?3:2;for(int y=0;y<n;++y)for(int x=0;x<n;++x){int idx=y*3+x;if(inside(px+70+x*(s+g),py+80+y*(s+g))){clickStack(crafting_[idx],cursor_,right);return true;}}
    for(int y=0;y<3;++y)for(int x=0;x<9;++x)if(inside(start+x*(s+g),py+300+y*(s+g))){int index=9+y*9+x;if(shift)return quickMove(index,0,9);clickStack(slots_[index],cursor_,right);return true;}
    for(int x=0;x<9;++x)if(inside(start+x*(s+g),py+470)){if(shift)return quickMove(x,9,36);clickStack(slots_[x],cursor_,right);return true;}
    return false;
}ItemStack Inventory::takeCursor(bool one){
    ItemStack result;if(cursor_.empty())return result;if(one){result=cursor_;result.count=1;--cursor_.count;normalize(cursor_);}else{result=cursor_;cursor_.clear();}return result;
}
void Inventory::returnCraftingItems(){for(auto& s:crafting_)if(!s.empty()){int left=add(s.item,s.count,s.durability);s.count=left;normalize(s);}}

bool Inventory::save(const std::string& path,std::uint32_t seed) const{
    std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)return false;f.write(InventoryMagic,8);writeValue(f,seed);writeValue(f,selectedSlot_);
    auto writeStack=[&](const ItemStack&s){std::uint8_t i=static_cast<std::uint8_t>(s.item);std::int16_t c=static_cast<std::int16_t>(s.count),d=static_cast<std::int16_t>(s.durability);return writeValue(f,i)&&writeValue(f,c)&&writeValue(f,d);};
    for(const auto&s:slots_)if(!writeStack(s))return false;for(const auto&s:crafting_)if(!writeStack(s))return false;if(!writeStack(cursor_))return false;return !!f;
}
bool Inventory::load(const std::string& path,std::uint32_t seed){
    std::ifstream f(path,std::ios::binary);if(!f)return false;char magic[8]{};f.read(magic,8);std::uint32_t stored=0;if(std::memcmp(magic,InventoryMagic,8)!=0||!readValue(f,stored)||stored!=seed||!readValue(f,selectedSlot_))return false;
    auto readStack=[&](ItemStack&s){std::uint8_t i;std::int16_t c,d;if(!readValue(f,i)||!readValue(f,c)||!readValue(f,d)||i>=static_cast<std::uint8_t>(Item::Count))return false;s={static_cast<Item>(i),c,d};normalize(s);return true;};
    for(auto&s:slots_)if(!readStack(s))return false;for(auto&s:crafting_)if(!readStack(s))return false;if(!readStack(cursor_))return false;selectSlot(selectedSlot_);return true;
}

Block itemToBlock(Item i){switch(i){case Item::Grass:return Block::Grass;case Item::Dirt:return Block::Dirt;case Item::Stone:return Block::Stone;case Item::Sand:return Block::Sand;case Item::Log:return Block::Log;case Item::Leaves:return Block::Leaves;case Item::Water:return Block::Water;case Item::CoalOre:return Block::CoalOre;case Item::IronOre:return Block::IronOre;case Item::GoldOre:return Block::GoldOre;case Item::CopperOre:return Block::CopperOre;case Item::DiamondOre:return Block::DiamondOre;case Item::Planks:return Block::Planks;case Item::CraftingTable:return Block::CraftingTable;case Item::Torch:return Block::Torch;default:return Block::Air;}}
Item blockToItem(Block b){switch(b){case Block::Grass:return Item::Grass;case Block::Dirt:return Item::Dirt;case Block::Stone:return Item::Stone;case Block::Sand:return Item::Sand;case Block::Log:return Item::Log;case Block::Leaves:return Item::Leaves;case Block::Water:return Item::Water;case Block::CoalOre:return Item::CoalOre;case Block::IronOre:return Item::IronOre;case Block::GoldOre:return Item::GoldOre;case Block::CopperOre:return Item::CopperOre;case Block::DiamondOre:return Item::DiamondOre;case Block::Planks:return Item::Planks;case Block::CraftingTable:return Item::CraftingTable;case Block::Torch:return Item::Torch;default:return Item::None;}}
bool isTool(Item i){return isPickaxe(i)||isAxe(i)||isShovel(i);}
bool isFood(Item i){return i==Item::Bread||i==Item::RawMeat;}
float foodValue(Item i){return i==Item::Bread?5.0f:(i==Item::RawMeat?3.0f:0.0f);}
float blockHardness(Block b){switch(b){case Block::Leaves:case Block::Torch:return .15f;case Block::Dirt:case Block::Grass:case Block::Sand:return .45f;case Block::Log:case Block::Planks:case Block::CraftingTable:return .9f;case Block::Stone:return 1.5f;case Block::CoalOre:case Block::CopperOre:return 2.0f;case Block::IronOre:return 2.5f;case Block::GoldOre:return 3.0f;case Block::DiamondOre:return 4.0f;default:return .2f;}}
float toolBreakMultiplier(Item i,Block b){int t=tier(i);if(t==0)return 1.0f;float m=2.0f+t*1.5f;if(isPickaxe(i)&&(b==Block::Stone||b==Block::CoalOre||b==Block::CopperOre||b==Block::IronOre||b==Block::GoldOre||b==Block::DiamondOre))return m;if(isAxe(i)&&(b==Block::Log||b==Block::Planks||b==Block::CraftingTable))return m;if(isShovel(i)&&(b==Block::Dirt||b==Block::Grass||b==Block::Sand))return m;return 1.0f;}
glm::vec3 itemColor(Item i){switch(i){case Item::Grass:return{.35f,.72f,.25f};case Item::Dirt:return{.48f,.31f,.16f};case Item::Stone:return{.5f,.5f,.5f};case Item::Sand:return{.86f,.78f,.48f};case Item::Log:return{.42f,.25f,.1f};case Item::Leaves:return{.2f,.62f,.18f};case Item::Water:return{.15f,.42f,.9f};case Item::CoalOre:return{.18f,.18f,.18f};case Item::IronOre:return{.72f,.57f,.45f};case Item::GoldOre:return{1.f,.75f,.12f};case Item::CopperOre:return{.76f,.38f,.2f};case Item::DiamondOre:return{.15f,.9f,.9f};case Item::Planks:return{.68f,.47f,.22f};case Item::Stick:return{.5f,.3f,.12f};case Item::CraftingTable:return{.62f,.39f,.18f};case Item::Torch:return{1.f,.68f,.1f};case Item::Seeds:return{.38f,.65f,.15f};case Item::Wheat:return{.92f,.74f,.18f};case Item::Bread:return{.82f,.52f,.16f};case Item::RawMeat:return{.8f,.25f,.28f};default:if(isTool(i))return{.72f,.72f,.68f};return{.7f,.7f,.7f};}}

SurvivalWorld::SurvivalWorld(std::uint32_t seed):seed_(seed){}
std::int64_t SurvivalWorld::chunkKey(int x,int z){return (static_cast<std::int64_t>(x)<<32)^static_cast<std::uint32_t>(z);}
void SurvivalWorld::spawnDrop(const glm::vec3&p,Item item,int amount,int durability){if(item==Item::None||amount<=0)return;Drop d;d.position=p;d.velocity={.4f,.8f,.2f};d.stack={item,amount,isTool(item)?(durability<0?Inventory::maxDurability(item):durability):0};drops_.push_back(d);}
void SurvivalWorld::spawnExperience(const glm::vec3&p,int amount){for(int i=0;i<std::max(1,amount/2);++i){ExperienceOrb o;o.position=p+glm::vec3((i%3-.8f)*.12f,.2f,(i%2-.5f)*.15f);o.velocity={0,.7f,0};o.value=std::max(1,amount/std::max(1,amount/2));experienceOrbs_.push_back(o);}}

void SurvivalWorld::spawnNearbyAnimals(World& world,const glm::vec3& p){
    int cx=static_cast<int>(std::floor(p.x/CHUNK_SIZE)),cz=static_cast<int>(std::floor(p.z/CHUNK_SIZE));
    for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx){int x=cx+dx,z=cz+dz;auto key=chunkKey(x,z);if(!spawnedChunks_.insert(key).second)continue;std::mt19937 rng(seed_^static_cast<unsigned>(x*92837111)^static_cast<unsigned>(z*689287499));std::uniform_int_distribution<int> pos(1,CHUNK_SIZE-2),chance(0,99);if(chance(rng)>38)continue;int wx=x*CHUNK_SIZE+pos(rng),wz=z*CHUNK_SIZE+pos(rng),y=world.terrainHeight(wx,wz);Block ground=world.getBlock(wx,y,wz);if(ground!=Block::Grass&&ground!=Block::Sand)continue;Animal a;a.position={wx+.5f,y+1.05f,wz+.5f};int t=chance(rng)%4;a.type=ground==Block::Sand?AnimalType::Pig:static_cast<AnimalType>(t);a.health=a.type==AnimalType::Wolf?8.f:10.f;a.thinkTimer=.2f*(chance(rng)%10);animals_.push_back(a);}
}
void SurvivalWorld::updateAnimal(Animal& a,float dt,World& world,Player& player,float daylight){
    a.thinkTimer-=dt;a.damageCooldown=std::max(0.f,a.damageCooldown-dt);a.loveTimer=std::max(0.f,a.loveTimer-dt);a.breedingCooldown=std::max(0.f,a.breedingCooldown-dt);a.age=std::min(0.f,a.age+dt);
    glm::vec3 delta=player.position()-a.position;float dist=glm::length(delta);bool hostile=a.type==AnimalType::Wolf&&daylight<.25f;
    if(hostile&&dist<9.f){glm::vec2 d=glm::normalize(glm::vec2(delta.x,delta.z));a.heading=d;if(dist<1.25f&&a.damageCooldown<=0){player.damage(2.f);a.damageCooldown=1.2f;}}
    else if(a.thinkTimer<=0){float angle=std::fmod(std::abs(std::sin(a.position.x*13.1f+a.position.z*7.3f+a.age))*31.4f,6.283f);a.heading={std::cos(angle),std::sin(angle)};a.thinkTimer=2.f+std::fmod(std::abs(a.position.x+a.position.z),3.f);}
    float speed=hostile?2.2f:.75f;glm::vec3 step={a.heading.x*speed*dt,0,a.heading.y*speed*dt};glm::vec3 next=a.position+step;glm::ivec3 ahead=glm::floor(next+glm::vec3(0,.2f,0));if(world.isSolidAt(ahead.x,ahead.y,ahead.z)){int top=world.terrainHeight(ahead.x,ahead.z);if(top+1.0f-next.y<1.1f)next.y=top+1.05f;else{a.heading=-a.heading;next=a.position;}}
    int ground=world.terrainHeight(static_cast<int>(std::floor(next.x)),static_cast<int>(std::floor(next.z)));next.y=ground+1.05f;a.position=next;
}
int SurvivalWorld::targetedAnimal(const glm::vec3&o,const glm::vec3&d,float reach) const{int best=-1;float bestT=std::numeric_limits<float>::max();for(int i=0;i<(int)animals_.size();++i){float t;if(lineOfSightTarget(o,d,animals_[i].position+glm::vec3(0,.55f,0),reach,.65f,t)&&t<bestT){best=i;bestT=t;}}return best;}
bool SurvivalWorld::feedAnimal(const glm::vec3&o,const glm::vec3&d,Item food){
    int idx=targetedAnimal(o,d,5.f);if(idx<0)return false;Animal&a=animals_[idx];bool ok=(a.type==AnimalType::Cow||a.type==AnimalType::Sheep)?food==Item::Wheat:(a.type==AnimalType::Pig?food==Item::Seeds:food==Item::RawMeat);if(!ok||a.breedingCooldown>0||a.age<0)return false;a.loveTimer=12.f;
    for(int i=0;i<(int)animals_.size();++i)if(i!=idx&&animals_[i].type==a.type&&animals_[i].loveTimer>0&&animals_[i].breedingCooldown<=0&&animals_[i].age>=0&&glm::distance(animals_[i].position,a.position)<5.f){Animal baby=a;baby.position=(a.position+animals_[i].position)*.5f;baby.age=-60.f;baby.loveTimer=0;baby.breedingCooldown=0;animals_[i].loveTimer=0;animals_[i].breedingCooldown=45.f;a.loveTimer=0;a.breedingCooldown=45.f;animals_.push_back(baby);spawnExperience(baby.position,3);break;}return true;
}
bool SurvivalWorld::attackAnimal(const glm::vec3&o,const glm::vec3&d,Item held){int idx=targetedAnimal(o,d,5.f);if(idx<0)return false;float damage=isAxe(held)?5.f:(isTool(held)?3.f:1.f);animals_[idx].health-=damage;if(animals_[idx].health<=0){glm::vec3 p=animals_[idx].position;spawnDrop(p,Item::RawMeat,1+(idx&1));spawnExperience(p,4);animals_.erase(animals_.begin()+idx);}return true;}

void SurvivalWorld::update(float dt,World& world,Player& player,Inventory& inventory,float daylight){
    spawnNearbyAnimals(world,player.position());for(auto&a:animals_)updateAnimal(a,dt,world,player,daylight);
    for(auto&d:drops_){d.age+=dt;d.velocity.y-=9.8f*dt;glm::vec3 n=d.position+d.velocity*dt;if(world.isSolidAt((int)std::floor(n.x),(int)std::floor(n.y),(int)std::floor(n.z))){d.velocity={0,0,0};}else d.position=n;if(glm::distance(d.position,player.position()+glm::vec3(0,.8f,0))<1.4f){int left=inventory.add(d.stack.item,d.stack.count,d.stack.durability);d.stack.count=left;}}
    drops_.erase(std::remove_if(drops_.begin(),drops_.end(),[](const Drop&d){return d.age>180.f||d.stack.count<=0;}),drops_.end());
    for(auto&o:experienceOrbs_){o.age+=dt;glm::vec3 target=player.position()+glm::vec3(0,.8f,0),delta=target-o.position;float dist=glm::length(delta);if(dist<7.f&&dist>.01f)o.velocity+=glm::normalize(delta)*(10.f*dt);o.velocity*=std::pow(.45f,dt);o.position+=o.velocity*dt;if(dist<.75f){player.addExperience(o.value);o.value=0;}}
    experienceOrbs_.erase(std::remove_if(experienceOrbs_.begin(),experienceOrbs_.end(),[](const ExperienceOrb&o){return o.age>120.f||o.value<=0;}),experienceOrbs_.end());
    if(animals_.size()>90)animals_.erase(animals_.begin(),animals_.begin()+(animals_.size()-90));
}
std::vector<RenderCuboid> SurvivalWorld::renderCuboids() const{
    std::vector<RenderCuboid> out;out.reserve(animals_.size()*6+experienceOrbs_.size());for(const auto&a:animals_){float baby=a.age<0?.55f:1.f;glm::vec3 c=a.type==AnimalType::Cow?glm::vec3(.35f,.2f,.12f):(a.type==AnimalType::Pig?glm::vec3(.95f,.55f,.6f):(a.type==AnimalType::Sheep?glm::vec3(.9f):glm::vec3(.38f,.38f,.4f)));out.push_back({a.position+glm::vec3(0,.55f*baby,0),glm::vec3(1.0f,.85f,.55f)*baby,c});out.push_back({a.position+glm::vec3(a.heading.x*.42f,.75f*baby,a.heading.y*.42f),glm::vec3(.55f)*baby,c*1.08f});for(int i=0;i<4;++i)out.push_back({a.position+glm::vec3((i<2?-.3f:.3f)*baby,.18f,(i%2?-.22f:.22f)*baby),glm::vec3(.16f,.48f,.16f)*baby,c*.75f});}
    for(const auto&o:experienceOrbs_)out.push_back({o.position,glm::vec3(.14f),glm::vec3(.35f,1.f,.08f)});return out;
}

std::vector<RenderItemSprite> SurvivalWorld::renderItemSprites() const{std::vector<RenderItemSprite> out;out.reserve(drops_.size());for(const auto&d:drops_)out.push_back({d.position,d.stack.item,.42f});return out;}
