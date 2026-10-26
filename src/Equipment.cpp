#include "Survival.h"
#include "Player.h"
#include "Definitions.h"
#include "Farming.h"
#include <algorithm>
#include <cmath>

bool isDurable(Item item) { return Inventory::maxDurability(item)>0; }
bool hasMainHandUse(Item item) {
    return (item>=Item::OakBoat && item<=Item::Compass) || isFood(item) ||
        itemToBlock(item)!=Block::Air || FarmingSystem::plantedBlock(item)!=Block::Air ||
        item==Item::Wheat || item==Item::Bucket || item==Item::BoneMeal ||
        itemDefinition(item).tool==ToolKind::Hoe || itemDefinition(item).tool==ToolKind::Shovel ||
        itemDefinition(item).tool==ToolKind::Axe || itemDefinition(item).tool==ToolKind::Shears;
}
int armorSlot(Item item) {
    const int index=int(item)-int(Item::LeatherHelmet);
    return index>=0 && index<16 ? index%4 : -1;
}
int armorDefense(Item item) {
    constexpr int points[4][4]={{1,3,2,1},{2,5,3,1},{2,6,5,2},{3,8,6,3}};
    const int index=int(item)-int(Item::LeatherHelmet);
    return index>=0 && index<16 ? points[index/4][index%4] : 0;
}
const ItemStack& Inventory::equipment(int index) const {
    static const ItemStack empty;
    return index>=0 && index<EquipmentSlots ? equipment_[index] : empty;
}
bool Inventory::clickEquipment(int index,bool right,bool shift) {
    if(index<0 || index>=EquipmentSlots) return false;
    cachedArmorPoints_=-1;
    auto& target=equipment_[index];
    if(shift) { moveExternalToInventory(target); return true; }
    if(!cursor_.empty() && index<4 && armorSlot(cursor_.item)!=index) return true;
    clickStack(target,cursor_,right);
    return true;
}
bool Inventory::equipFromSlot(int index) {
    if(hotbarLocked(index))return false;
    if(index<0 || index>=TotalSlots || slots_[index].empty()) return false;
    auto& source=slots_[index];
    const int target=source.item==Item::Shield ? OffhandSlot : armorSlot(source.item);
    if(target<0) return false;
    // Quick equip never displaces occupied equipment; manual clicks can swap.
    // Let the caller perform its normal hotbar/container transfer when occupied.
    if(!equipment_[target].empty()) return false;
    equipment_[target]=source; source.clear(); cachedArmorPoints_=-1;
    return true;
}
void Inventory::swapOffhand() { std::swap(slots_[selectedSlot_],equipment_[OffhandSlot]); }
ItemStack Inventory::takeEquipment(int index,bool entireStack) {
    if(index<0 || index>=EquipmentSlots) return {};
    cachedArmorPoints_=-1;
    auto result=equipment_[index];
    if(entireStack) equipment_[index].clear();
    else { result.count=std::min(1,result.count);equipment_[index].count-=result.count;normalize(equipment_[index]); }
    return result;
}
bool Inventory::consumeOffhand(int count) {
    auto& stack=equipment_[4];if(stack.count<count)return false;
    stack.count-=count;normalize(stack);return true;
}
bool Inventory::damageOffhandTool(int amount) {
    auto& stack=equipment_[OffhandSlot];
    if(stack.empty() || !isTool(stack.item) || amount<=0) return false;
    stack.durability-=amount;
    if(stack.durability<=0) {stack.clear();equipmentBroke_=true;}
    return true;
}
int Inventory::armorPoints() const {
    if(cachedArmorPoints_>=0) return cachedArmorPoints_;
    int points=0;
    for(int i=0;i<4;++i) if(!equipment_[i].empty()) points+=armorDefense(equipment_[i].item);
    return cachedArmorPoints_=points;
}
float Inventory::mitigatePhysical(float damage,bool blockable,const glm::vec3& incoming,
                                  const glm::vec3& facing,bool blocking,bool& blocked) {
    blocked=false;
    auto& shield=equipment_[OffhandSlot];
    const float distanceSquared=glm::dot(incoming,incoming);
    if(blockable && blocking && shield.item==Item::Shield && !shield.empty() &&
       distanceSquared>1e-6f && glm::dot(incoming/std::sqrt(distanceSquared),facing)>=.5f) {
        blocked=true;
        shield.durability-=std::max(1,int(std::ceil(damage)));
        if(shield.durability<=0) {shield.clear();equipmentBroke_=true;}
        return 0;
    }
    const float armor=float(armorPoints());
    // Damage-dependent armor effectiveness; protection is capped at 80%.
    const float effective=std::clamp(std::max(armor*.2f,armor-damage*.5f),0.f,20.f);
    const float result=damage*(1.f-effective*.04f);
    const int wear=std::max(1,int(std::floor(damage/4.f)));
    for(int i=0;i<4;++i) if(!equipment_[i].empty()) {
        equipment_[i].durability-=wear;
        if(equipment_[i].durability<=0) {equipment_[i].clear();cachedArmorPoints_=-1;equipmentBroke_=true;}
    }
    return result;
}
void Player::setBlocking(bool enabled) { blocking_=enabled; }
bool Player::isBlocking() const {
    return blocking_ && !dead_ && !spectatorMode_ && equipment_ &&
        !equipment_->equipment(4).empty() && equipment_->equipment(4).item==Item::Shield;
}
bool Player::damagePhysical(float amount,PlayerDamageSource source,const glm::vec3& origin) {
    if(source!=PlayerDamageSource::Melee && source!=PlayerDamageSource::Projectile &&
       source!=PlayerDamageSource::Explosion) {damage(amount,source);return false;}
    // Preserve source-specific invulnerability before wearing any equipment.
    if(creativeMode_ || spectatorMode_ || dead_ || respawnProtection_>0 || amount<=0 ||
        (source!=PlayerDamageSource::Projectile && damageInvulnerability_>0)) return false;
    bool blocked=false;
    if(equipment_) amount=equipment_->mitigatePhysical(amount,true,
        origin-(position_+glm::vec3(0,Height*.5f,0)),lookDirection(),isBlocking(),blocked);
    if(blocked) {
        shieldFeedback_=.28f;
        if(source!=PlayerDamageSource::Projectile) damageInvulnerability_=.65f;
        return true;
    }
    damage(amount,source);
    return false;
}
