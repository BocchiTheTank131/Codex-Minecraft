#include "Survival.h"
#include "Player.h"
#include "Definitions.h"
#include "UiLayout.h"
#include "ExplorationLoot.h"
#include "Persistence.h"
#include "World.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>

bool Inventory::runEquipmentSelfTest(std::string& report) {
    try {
        const auto check=[](bool value,const char* label) {if(!value)throw std::runtime_error(label);};
        Inventory inventory;
        for(const auto& stack:inventory.equipment_) check(stack.empty(),"initial equipment");
        check(hasMainHandUse(Item::Seeds)&&hasMainHandUse(Item::Wheat)&&
            hasMainHandUse(Item::IronShovel)&&hasMainHandUse(Item::Bread)&&
            hasMainHandUse(Item::Torch)&&!hasMainHandUse(Item::IronSword),"primary use priority");
        inventory.add(Item::IronShovel,1,1);inventory.swapOffhand();inventory.add(Item::IronSword,1,80);
        check(inventory.damageOffhandTool()&&inventory.equipment(4).empty()&&
            inventory.selectedStack().durability==80&&inventory.takeEquipmentBreak(),"offhand tool wear ownership");
        const int setPoints[]={7,11,15,20};
        for(int material=0;material<4;++material) {
            inventory.clear();
            for(int part=0;part<4;++part) {
                const auto item=static_cast<Item>(int(Item::LeatherHelmet)+material*4+part);
                Item restored;ItemSpriteUv uv;
                check(itemFromSaveId(itemDefinition(item).saveId,restored)&&restored==item&&itemSpriteUv(item,uv),"equipment IDs/UV");
                inventory.add(item,1,10);check(inventory.equipFromSlot(0),"quick equip");
                check(inventory.equipment(part).durability==10&&inventory.slot(0).empty(),"equip transfer durability");
                bool recipeFound=false;
                for(int i=0;i<int(craftingRecipes().size());++i) if(craftingRecipes()[i].output==item) {
                    Inventory craft;const auto ingredient=std::array<Item,4>{Item::Leather,Item::GoldIngot,Item::IronIngot,Item::Diamond}[material];
                    craft.add(ingredient,24);check(craft.fillRecipe(i,true,false),"armor recipe fill");
                    check(craft.craftingOutput(true).item==item&&craft.craftingOutput(true).durability==maxDurability(item),"armor recipe output");recipeFound=true;
                }
                check(recipeFound,"missing armor recipe");
            }
            check(inventory.armorPoints()==setPoints[material],"full set protection");
        }
        // Every possible mixed-material/empty combination, plus exact cap and wear.
        for(int combination=0;combination<625;++combination) {
            inventory.clear();int code=combination,points=0;
            for(int part=0;part<4;++part) {
                const int material=code%5;code/=5;
                if(material<4) {
                    const auto item=static_cast<Item>(int(Item::LeatherHelmet)+material*4+part);
                    inventory.equipment_[part]={item,1,10};points+=armorDefense(item);
                }
            }
            bool blocked=false;
            const float result=inventory.mitigatePhysical(8,false,{0,0,-1},{0,0,-1},false,blocked);
            const float expected=8*(1-std::max(points*.2f,points-4.f)*.04f);
            check(std::abs(result-expected)<1e-5f&&!blocked,"mixed armor calculation");
            for(const auto& stack:inventory.equipment_) if(!stack.empty())check(stack.durability==8,"armor wear");
        }
        inventory.clear();inventory.equipment_[0]={Item::DiamondHelmet,1,1};bool blocked=false;
        inventory.mitigatePhysical(4,false,{0,0,-1},{0,0,-1},false,blocked);
        check(inventory.equipment(0).empty(),"armor break");
        for(const auto source:{PlayerDamageSource::Melee,PlayerDamageSource::Projectile,PlayerDamageSource::Explosion}) {
            for(int direction=0;direction<3;++direction) {
                Inventory gear;gear.equipment_[4]={Item::Shield,1,100};
                Player victim({0,0,0});victim.setEquipment(&gear);victim.setBlocking(true);
                const auto origin=glm::vec3(direction==2?5.f:0.f,.9f,direction==0?-5.f:direction==1?5.f:0.f);
                const bool stopped=victim.damagePhysical(4,source,origin);
                check(stopped==(direction==0),"directional shield");
                check(victim.health()==(stopped?20.f:16.f),"shield damage");
                check(gear.equipment(4).durability==(stopped?96:100),"shield wear");
            }
        }
        for(const auto source:{PlayerDamageSource::Burning,PlayerDamageSource::Fall,PlayerDamageSource::Starvation,PlayerDamageSource::General}) {
            Inventory gear;gear.equipment_[4]={Item::Shield,1,100};gear.equipment_[1]={Item::DiamondChestplate,1,100};
            Player victim({0,0,0});victim.setEquipment(&gear);victim.setBlocking(true);victim.damage(5,source);
            check(victim.health()==15&&gear.equipment(4).durability==100&&gear.equipment(1).durability==100,"environment bypass");
            Player routed({0,0,0});routed.setEquipment(&gear);routed.setBlocking(true);
            check(!routed.damagePhysical(5,source,{0,.9f,-5})&&routed.health()==15,"environment routing bypass");
        }
        inventory.clear();inventory.setCreativeCursor(Item::Shield);
        inventory.clickEquipment(4,false);
        for(int i=0;i<int(craftingRecipes().size());++i) if(craftingRecipes()[i].output==Item::Shield) {
            Inventory craft;craft.add(Item::SprucePlanks,6);craft.add(Item::IronIngot,1);
            check(craft.fillRecipe(i,true,false)&&craft.craftingOutput(true).item==Item::Shield,"shield plank-group recipe");
        }
        for(Item sword:{Item::WoodSword,Item::StoneSword,Item::IronSword,Item::GoldSword,Item::DiamondSword}) {
            Inventory weapon;weapon.add(sword,1,1);
            check(attackCooldown(sword)==.625f&&attackDamage(sword)>=4&&attackDamage(sword)<=7,"sword balance/cooldown");
            check(weapon.damageSelectedTool()&&weapon.selectedStack().empty(),"weapon break");
        }
        inventory.clear();inventory.equipment_[4]={Item::Shield,1,1};
        Player victim({0,0,0});victim.setEquipment(&inventory);victim.setBlocking(true);
        check(victim.damagePhysical(4,PlayerDamageSource::Melee,{0,.9f,-5})&&inventory.equipment(4).empty()&&!victim.isBlocking(),"shield break");
        inventory.equipment_[4]={Item::Shield,1,100};
        victim.damagePhysical(4,PlayerDamageSource::Melee,{0,.9f,-5});
        check(inventory.equipment(4).durability==100,"invulnerable wear");
        victim.setCreativeMode(true);victim.damagePhysical(4,PlayerDamageSource::Projectile,{0,.9f,-5});
        check(inventory.equipment(4).durability==100,"creative wear");
        // Normal clicks, swaps, invalid item rejection, partial offhand drops and full inventory.
        inventory.clear();inventory.add(Item::IronBoots,1,81);
        const auto owned=UiLayout::playerSlot(UiMode::Inventory,0,1280,720);
        check(inventory.handleInventoryClick(owned.x+20,owned.y+20,1280,720,false,false,true)&&
            inventory.equipment(3).durability==81&&inventory.slot(0).empty(),"GUI shift equip");
        inventory.add(Item::LeatherBoots,1,23);
        check(inventory.handleInventoryClick(owned.x+20,owned.y+20,1280,720,false,false,true)&&
            inventory.equipment(3).item==Item::IronBoots&&inventory.slot(0).empty()&&
            inventory.slot(9).item==Item::LeatherBoots&&inventory.slot(9).durability==23,"occupied armor shift transfer");
        inventory.clear();inventory.add(Item::Shield,1,81);
        const auto creativeOwned=UiLayout::playerSlot(UiMode::Creative,0,1280,720);
        check(inventory.handleCreativeClick(creativeOwned.x+20,creativeOwned.y+20,1280,720,false,true)&&
            inventory.equipment(4).durability==81&&inventory.slot(0).empty(),"Creative owned shift equip");
        const auto armorCatalog=UiLayout::creativeItem(int(Item::IronHelmet)-1,1280,720);
        check(inventory.handleCreativeClick(armorCatalog.x+15,armorCatalog.y+15,1280,720,false,true,(int(Item::IronHelmet)-1)/UiLayout::CreativePageSize)&&
            inventory.equipment(0).item==Item::IronHelmet&&inventory.cursor_.empty(),"Creative catalog shift equip");
        inventory.clear();inventory.setCreativeCursor(Item::IronHelmet);
        inventory.clickEquipment(1,false);check(!inventory.cursor_.empty()&&inventory.equipment(1).empty(),"wrong slot");
        inventory.clickEquipment(0,false);check(inventory.cursor_.empty()&&inventory.equipment(0).item==Item::IronHelmet,"cursor equip");
        inventory.setCreativeCursor(Item::LeatherHelmet);inventory.clickEquipment(0,false);
        check(inventory.cursor_.item==Item::IronHelmet&&inventory.equipment(0).item==Item::LeatherHelmet,"armor swap");
        inventory.cursor_.clear();inventory.clickEquipment(0,false,true);
        check(inventory.equipment(0).empty()&&inventory.count(Item::LeatherHelmet)==1,"unequip shift");
        inventory.setCreativeCursor(Item::Torch);inventory.clickEquipment(4,false);
        check(inventory.takeGuiDrop({UiSlotKind::EquipmentSlot,4},false).count==1&&inventory.equipment(4).count==63,"single offhand drop");
        check(inventory.takeGuiDrop({UiSlotKind::EquipmentSlot,4},true).count==63&&inventory.equipment(4).empty(),"full offhand drop");
        inventory.clear();inventory.add(Item::Shield,1,81);inventory.swapOffhand();inventory.swapOffhand();
        check(inventory.selectedStack().durability==81&&inventory.equipment(4).empty(),"F swap roundtrip");
        inventory.equipFromSlot(0);for(int i=0;i<TotalSlots;++i)inventory.slots_[i]={Item::Stone,64,0};
        inventory.clickEquipment(4,false,true);check(inventory.equipment(4).durability==81,"full inventory unequip");
        for(const auto mode:{UiMode::Inventory,UiMode::Creative,UiMode::CraftingTable,UiMode::Chest,UiMode::Furnace})
            for(int i=0;i<5;++i) {
                const auto b=UiLayout::equipmentSlot(mode,i,1280,720);
                check(UiLayout::hit(mode,b.x+20,b.y+20,1280,720,creativeCatalog().size()).kind==UiSlotKind::EquipmentSlot,"equipment hit layout");
            }
        if(Persistence::enabled()) {
            const std::string path="equipment_test.vxi";
            for(int part=0;part<4;++part)
                inventory.equipment_[part]={static_cast<Item>(int(Item::LeatherHelmet)+part*5),1,11+part};
            inventory.cachedArmorPoints_=-1;
            check(inventory.save(path,99),"equipment save");Inventory restored;
            check(restored.load(path,99)&&restored.equipment(4).durability==81,"equipment reload");
            for(int part=0;part<4;++part)
                check(restored.equipment(part).item==inventory.equipment(part).item &&
                    restored.equipment(part).durability==11+part,"mixed equipped armor reload");
            const auto before=restored.equipment(4);
            std::ifstream in(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});in.close();
            std::ofstream truncated(path,std::ios::binary|std::ios::trunc);truncated.write(bytes.data(),bytes.size()-1);truncated.close();
            check(!restored.load(path,99)&&restored.equipment(4).durability==before.durability,"transactional truncated inventory");
            // Construct exact historical V4 records independently of the current
            // 16-bit writer: byte ID, 16-bit count/durability, no identity/gear.
            std::string legacy=bytes.substr(0,16);legacy[5]='4';
            auto appendLegacy=[&](const ItemStack& stack){const auto id=std::uint8_t(stack.item);const auto count=std::int16_t(stack.count),durability=std::int16_t(stack.durability);legacy.append(reinterpret_cast<const char*>(&id),1);legacy.append(reinterpret_cast<const char*>(&count),2);legacy.append(reinterpret_cast<const char*>(&durability),2);};
            for(int slot=0;slot<TotalSlots;++slot)appendLegacy(inventory.slot(slot));
            for(int slot=0;slot<CraftSlots;++slot)appendLegacy(inventory.craftSlot(slot));
            appendLegacy(inventory.cursorStack());
            bytes=std::move(legacy);
            check(bytes.size()==16+(TotalSlots+CraftSlots+1)*5,"V4 fixture record lengths");
            std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());out.close();
            check(restored.load(path,99)&&restored.equipment(4).empty()&&restored.slot(0).count==64,"V4 migration");
            for(int part=0;part<4;++part) check(restored.equipment(part).empty(),"V4 empty armor");
            std::filesystem::remove(path);
        }
        int deepGear=0,shallowGear=0,diamonds=0;
        for(int i=0;i<10000;++i) for(int depth:{64,-60}) {
            StructureLootMarker marker{i,depth,17,StructureLoot::Ancient,unsigned(i)};
            auto a=rollExplorationLoot(99,marker),b=a;addEquipmentLoot(a,99,marker);addEquipmentLoot(b,99,marker);
            for(int slot=0;slot<27;++slot) {
                check(a[slot].item==b[slot].item&&a[slot].durability==b[slot].durability,"loot determinism");
                if(armorSlot(a[slot].item)>=0||a[slot].item==Item::Shield) {
                    (depth<0?deepGear:shallowGear)++;
                    diamonds+=a[slot].item==Item::DiamondHelmet;
                    check(a[slot].count==1&&a[slot].durability>0&&a[slot].durability<maxDurability(a[slot].item),"loot durability");
                }
            }
        }
        check(deepGear>shallowGear&&diamonds>0&&diamonds<60,"loot depth/rarity");
        std::cout<<"Equipment loot (10,000 shallow/deep ancient chests): "<<shallowGear<<" / "<<deepGear<<", diamond helmets "<<diamonds<<'\n';
        report="16 armor pieces, 625 combinations, recipes, directional shields, environmental bypass, durability/breakage, UI swaps/drops, Creative ownership, legacy V4 migration and deterministic depth loot passed";
        return true;
    } catch(const std::exception& error) {report=error.what();return false;}
}

bool World::runEquipmentWorldSelfTest(std::string& report) {
    if(!Persistence::enabled()) {report="Demo save fixture skipped (stateless)";return true;}
    try {
        World original(99,21);
        original.generate(2,{.5f,110,.5f});
        original.setBlock(1,110,1,Block::Chest);
        Inventory equipment;
        equipment.add(Item::DiamondChestplate,1,87);equipment.equipFromSlot(0);
        equipment.add(Item::Shield,1,91);equipment.equipFromSlot(0);
        Player wearer({.5f,111,.5f});wearer.setEquipment(&equipment);wearer.setBlocking(true);
        wearer.damagePhysical(20,PlayerDamageSource::Burning,{.5f,111,-5});
        if(!wearer.isDead() || equipment.equipment(1).durability!=87 || equipment.equipment(4).durability!=91)
            throw std::runtime_error("Equipped environmental death changed");
        wearer.respawn(original);
        if(wearer.isDead() || wearer.health()!=20 || wearer.isBlocking() || equipment.equipment(1).durability!=87)
            throw std::runtime_error("Equipped respawn changed inventory retention");
        wearer.setSpectatorMode(true);wearer.setBlocking(true);
        wearer.damagePhysical(8,PlayerDamageSource::Projectile,{.5f,111,-5});
        if(wearer.health()!=20 || wearer.isBlocking() || equipment.equipment(4).durability!=91)
            throw std::runtime_error("Spectator equipment immunity");
        auto* chest=original.chestAt({1,110,1},true);
        chest->slots[0]={Item::IronHelmet,1,74};chest->slots[1]={Item::Shield,1,101};
        const std::string path="equipment_world_test.vxw";
        if(!original.saveWorld(path,{.5f,111,.5f})) throw std::runtime_error("World gear extension save");
        World restored(99,21);glm::vec3 position;
        if(!restored.loadWorld(path,position)||!restored.equipmentLoot_) throw std::runtime_error("World gear rules reload");
        restored.generate(2,position);
        auto* contents=restored.chestAt({1,110,1},false);
        if(!contents||contents->slots[0].durability!=74||contents->slots[1].durability!=101)
            throw std::runtime_error("Modified gear chest persistence");
        std::ifstream in(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});in.close();
        const auto gearOffset=bytes.rfind(std::string("VXGEAR1\0",8));
        if(gearOffset==std::string::npos)throw std::runtime_error("Missing gear-rule fixture boundary");
        bytes.resize(gearOffset); // Payload before optional gear/building extensions.
        std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());out.close();
        World legacy(99,21);
        if(!legacy.loadWorld(path,position)||legacy.equipmentLoot_) throw std::runtime_error("Legacy world loot rules changed");
        if(!legacy.saveWorld(path,position)||!restored.loadWorld(path,position)||restored.equipmentLoot_)
            throw std::runtime_error("Legacy loot rule persistence");
        std::filesystem::remove(path);
        report="World gear-rule migration, equipped death/respawn, Spectator immunity and modified gear chest persistence passed";
        return true;
    } catch(const std::exception& e) {report=e.what();return false;}
}
