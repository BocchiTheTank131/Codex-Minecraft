#include "World.h"
#include "Game.h"
#include "Player.h"
#include "Transportation.h"
#include "Weather.h"
#include "WorldTime.h"
#include "SaveFile.h"
#include "SaveValidation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
struct SavedEdit {std::int32_t x,z;std::uint32_t index;Block block;};
using FlatEdits=std::vector<SavedEdit>;
std::uint64_t savedEditCount(const FlatEdits& edits){return edits.size();}
template<class Edits> std::uint64_t savedEditCount(const Edits& edits) {
    std::uint64_t n=0;for(const auto& chunk:edits)n+=chunk.second.size();return n;
}
template<class Visitor> void eachSavedEdit(const FlatEdits& edits,Visitor visitor) {
    for(const auto& e:edits)visitor(e.x,e.z,e.index,e.block);
}
template<class Edits,class Visitor> void eachSavedEdit(const Edits& edits,Visitor visitor) {
    for(const auto& chunk:edits){const auto packed=std::uint64_t(chunk.first);
        const auto x=std::int32_t(packed>>32),z=std::int32_t(packed);
        for(const auto& e:chunk.second)visitor(x,z,std::uint32_t(e.first),e.second);
    }
}
template<class Edits,class Furnaces,class Chests,class Texts,class Drops,class AutomationWriter>
bool writeWorldData(const std::string& path,const glm::vec3& playerPosition,
    std::uint32_t seed_,unsigned generationVersion_,bool equipmentLoot_,int low,int high,
    const Edits& edits_,const Furnaces& furnaces_,const Chests& chests_,
    const Texts& buildingTexts_,const Drops& buildingDrops_,const Adventure& adventure,AutomationWriter automationWrite) {

    if(generationVersion_<24&&!adventure.puzzles().empty())return false;
    if(!SaveValidation::position(playerPosition) || buildingTexts_.size()>100000 || buildingDrops_.size()>100000)return false;
    for(const auto& drop:buildingDrops_)if(!SaveValidation::position(drop.first)||!SaveValidation::stack(drop.second)||drop.second.empty())return false;
    for(const auto& e:buildingTexts_){if(e.second.text.size()>99 || !SaveValidation::position(glm::vec3(e.first.x,e.first.y,e.first.z)) || e.first.y<low || e.first.y>=high)return false;int lines=1,width=0;for(unsigned char c:e.second.text){if(c=='\n'){++lines;width=0;}else if(c<32||c>126||++width>24)return false;}if(lines>4)return false;}
    const auto totalEdits=savedEditCount(edits_);bool validEdits=true;
    eachSavedEdit(edits_,[&](int,int,std::uint32_t,Block b){validEdits &= validBlockState(b);});if(!validEdits)return false;
    if(totalEdits>10000000 || furnaces_.size()>1000000 || chests_.size()>1000000)return false;
    for(const auto& entry:furnaces_){const auto& f=entry.second;
        if(!SaveValidation::stack(f.input)||!SaveValidation::stack(f.output)||!SaveValidation::stack(f.fuel)||!std::isfinite(f.fuelRemaining)||f.fuelRemaining<0||!std::isfinite(f.fuelCapacity)||f.fuelCapacity<0||!std::isfinite(f.progress)||f.progress<0||f.progress>1)return false;}
    for(const auto& c:chests_)for(const auto& stack:c.second.slots)if(!SaveValidation::stack(stack))return false;
    return SaveFile::write(path, std::ios::binary, [&](std::ofstream& output) {
    const char magic[8] = {'V', 'X', 'W', 'O', 'R', 'L', 'D', '3'};
    const std::uint32_t version = 3U;
    const std::uint32_t savedGenerationVersion = generationVersion_;
    const std::uint64_t editCount = savedEditCount(edits_);
    output.write(magic, sizeof(magic));
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    output.write(reinterpret_cast<const char*>(&seed_), sizeof(seed_));
    output.write(reinterpret_cast<const char*>(&playerPosition.x), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.y), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.z), sizeof(float));
    output.write(reinterpret_cast<const char*>(&editCount), sizeof(editCount));
    output.write(reinterpret_cast<const char*>(&savedGenerationVersion),
                 sizeof(savedGenerationVersion));
    constexpr std::size_t EditRecordBytes = sizeof(std::int32_t) * 2 +
                                            sizeof(std::uint32_t) + sizeof(std::uint32_t);
    thread_local std::array<char, 64 * 1024> editBuffer{};
    std::size_t buffered = 0;
    const auto flushEdits = [&] {
        if (buffered) output.write(editBuffer.data(), static_cast<std::streamsize>(buffered));
        buffered = 0;
    };
    const auto appendField = [&](const auto& value) {
        std::memcpy(editBuffer.data() + buffered, &value, sizeof(value));
        buffered += sizeof(value);
    };
    eachSavedEdit(edits_,[&](std::int32_t chunkX,std::int32_t chunkZ,std::uint32_t index,Block block) {
        if(buffered+EditRecordBytes>editBuffer.size())flushEdits();
        const std::uint32_t value=std::uint32_t(block);
        appendField(chunkX);appendField(chunkZ);appendField(index);appendField(value);
    });
    flushEdits();
    const char entityMagic[8] = {'V', 'X', 'E', 'N', 'T', 'S', '3', '\0'};
    output.write(entityMagic, sizeof(entityMagic));
    auto writeStack = [&](const ItemStack& stack) {
        const std::uint16_t item = itemDefinition(stack.item).saveId;
        const std::int16_t count = static_cast<std::int16_t>(stack.count);
        const std::int16_t durability = static_cast<std::int16_t>(stack.durability);
        output.write(reinterpret_cast<const char*>(&item), sizeof(item));
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        output.write(reinterpret_cast<const char*>(&durability), sizeof(durability));
        output.write(reinterpret_cast<const char*>(&stack.identity), sizeof(stack.identity));
    };
    const std::uint32_t furnaceCount = static_cast<std::uint32_t>(furnaces_.size());
    output.write(reinterpret_cast<const char*>(&furnaceCount), sizeof(furnaceCount));
    for (const auto& entry : furnaces_) {
        output.write(reinterpret_cast<const char*>(&entry.first.x), sizeof(entry.first.x));
        output.write(reinterpret_cast<const char*>(&entry.first.y), sizeof(entry.first.y));
        output.write(reinterpret_cast<const char*>(&entry.first.z), sizeof(entry.first.z));
        writeStack(entry.second.input);
        writeStack(entry.second.fuel);
        writeStack(entry.second.output);
        output.write(reinterpret_cast<const char*>(&entry.second.fuelRemaining), sizeof(float));
        output.write(reinterpret_cast<const char*>(&entry.second.fuelCapacity), sizeof(float));
        const float progress=entry.second.progressInput==entry.second.input.item?entry.second.progress:0.f;
        output.write(reinterpret_cast<const char*>(&progress), sizeof(float));
    }
    const std::uint32_t chestCount = static_cast<std::uint32_t>(chests_.size());
    output.write(reinterpret_cast<const char*>(&chestCount), sizeof(chestCount));
    for (const auto& entry : chests_) {
        output.write(reinterpret_cast<const char*>(&entry.first.x), sizeof(entry.first.x));
        output.write(reinterpret_cast<const char*>(&entry.first.y), sizeof(entry.first.y));
        output.write(reinterpret_cast<const char*>(&entry.first.z), sizeof(entry.first.z));
        for (const ItemStack& stack : entry.second.slots)
            writeStack(stack);
    }
    output.write("VXGEAR1",8);
    const std::uint8_t equipmentLoot=equipmentLoot_?1:0;
    output.write(reinterpret_cast<const char*>(&equipmentLoot),1);
    output.write("BLD1",4);const auto count=std::uint32_t(buildingTexts_.size());output.write(reinterpret_cast<const char*>(&count),4);
    for(const auto& e:buildingTexts_){output.write(reinterpret_cast<const char*>(&e.first.x),4);output.write(reinterpret_cast<const char*>(&e.first.y),4);output.write(reinterpret_cast<const char*>(&e.first.z),4);auto len=std::uint16_t(e.second.text.size());output.write(reinterpret_cast<const char*>(&len),2);output.write(e.second.text.data(),len);}
    output.write("BDP1",4);const auto pendingCount=std::uint32_t(buildingDrops_.size());output.write(reinterpret_cast<const char*>(&pendingCount),4);
    for(const auto& drop:buildingDrops_){output.write(reinterpret_cast<const char*>(&drop.first.x),4);output.write(reinterpret_cast<const char*>(&drop.first.y),4);output.write(reinterpret_cast<const char*>(&drop.first.z),4);writeStack(drop.second);}
    if(!automationWrite(output)||!adventure.write(output))return false;
    return output.good();
    });

}
} // namespace

bool World::saveWorld(const std::string& path,const glm::vec3& playerPosition) const {
    if(!automation.valid())return false;
    return writeWorldData(path,playerPosition,seed_,generationVersion_,equipmentLoot_,minimumY(),maximumY(),
        edits_,furnaces_,chests_,buildingTexts_,buildingDrops_,adventure,[&](std::ostream& out){return automation.write(out);});
}

std::function<bool()> World::captureSaveWorld(const std::string& path,const glm::vec3& playerPosition) const {
    struct Snapshot {
        Adventure adventure;
        FlatEdits edits;
        std::vector<std::pair<BlockEntityPosition,FurnaceData>> furnaces;
        std::vector<std::pair<BlockEntityPosition,ChestData>> chests;
        std::vector<std::pair<BlockEntityPosition,BuildingText>> texts;
        std::vector<std::pair<glm::vec3,ItemStack>> drops;
        std::function<bool(std::ostream&)> automation;
    };
    const auto n=savedEditCount(edits_);
    if(n>10000000||furnaces_.size()>1000000||chests_.size()>1000000||buildingTexts_.size()>100000||buildingDrops_.size()>100000)
        throw std::length_error("World snapshot exceeds save-format bounds");
    std::size_t bytes=sizeof(Snapshot)+std::size_t(n)*sizeof(SavedEdit)+furnaces_.size()*sizeof(decltype(Snapshot::furnaces)::value_type)+
        chests_.size()*sizeof(decltype(Snapshot::chests)::value_type)+buildingTexts_.size()*sizeof(decltype(Snapshot::texts)::value_type)+buildingDrops_.size()*sizeof(decltype(Snapshot::drops)::value_type);
    for(const auto& e:buildingTexts_)bytes+=e.second.text.size();
    SaveFile::reserveSnapshotBytes(bytes);
    SaveFile::reserveSnapshotBytes(adventure.discoveries().size()*sizeof(AdventureDiscovery)+adventure.puzzles().size()*64+4096);
    auto data=std::make_shared<Snapshot>();data->adventure=adventure;data->edits.reserve(std::size_t(n));
    eachSavedEdit(edits_,[&](std::int32_t x,std::int32_t z,std::uint32_t i,Block b){data->edits.push_back({x,z,i,b});});
    data->furnaces.assign(furnaces_.begin(),furnaces_.end());data->chests.assign(chests_.begin(),chests_.end());
    data->texts.assign(buildingTexts_.begin(),buildingTexts_.end());data->drops=buildingDrops_;
    data->automation=automation.captureWrite();
    std::shared_ptr<const Snapshot> frozen=std::move(data);
    const auto seed=seed_;const auto version=generationVersion_;const auto gear=equipmentLoot_;const int low=minimumY(),high=maximumY();
    return [frozen,path,playerPosition,seed,version,gear,low,high]{
        return writeWorldData(path,playerPosition,seed,version,gear,low,high,frozen->edits,frozen->furnaces,frozen->chests,
            frozen->texts,frozen->drops,frozen->adventure,frozen->automation);
    };
}

std::function<bool()> Inventory::captureSave(const std::string& path,std::uint32_t seed) const {
    SaveFile::reserveSnapshotBytes(sizeof(Inventory)+4096);
    std::shared_ptr<const Inventory> snapshot=std::make_shared<Inventory>(*this);
    return [snapshot,path,seed]{return snapshot->save(path,seed);};
}
std::function<bool()> Player::captureSave(const std::string& path,std::uint32_t seed) const {
    SaveFile::reserveSnapshotBytes(sizeof(Player)+4096);
    auto copy=std::make_shared<Player>(glm::vec3(0));
    copy->health_=health_;copy->hunger_=hunger_;copy->experience_=experience_;
    copy->saturation_=saturation_;copy->exhaustion_=exhaustion_;copy->regenTimer_=regenTimer_;
    copy->hasPersonalBed_=hasPersonalBed_;copy->personalBed_=personalBed_;
    std::shared_ptr<const Player> snapshot=std::move(copy);
    return [snapshot,path,seed]{return snapshot->save(path,seed);};
}
std::function<bool()> SurvivalWorld::captureSave(const std::string& path,std::uint32_t seed) const {
    SaveFile::reserveSnapshotBytes(sizeof(SurvivalWorld)+animals_.size()*sizeof(Animal)+drops_.size()*sizeof(Drop)+spawnedStructureMarkers_.size()*32);
    auto copy=std::make_shared<SurvivalWorld>(seed_);
    copy->animals_.reserve(animals_.size());
    for(const auto& a:animals_){Animal saved;saved.type=a.type;saved.relicEncounter=a.relicEncounter;saved.position=a.position;saved.health=a.health;
        saved.age=a.age;saved.deathTimer=a.deathTimer;saved.persistent=a.persistent;saved.sheared=a.sheared;
        saved.resourceTimer=a.resourceTimer;saved.woolTimer=a.woolTimer;saved.loveTimer=a.loveTimer;saved.breedingCooldown=a.breedingCooldown;
        copy->animals_.push_back(std::move(saved));}
    copy->drops_=drops_;copy->spawnedStructureMarkers_=spawnedStructureMarkers_;
    std::shared_ptr<const SurvivalWorld> snapshot=std::move(copy);
    return [snapshot,path,seed]{return snapshot->save(path,seed);};
}
std::function<bool()> Transportation::captureSave(const std::string& path,std::uint32_t seed) const {
    // Only already-published survey pixels belong to this coherent live state.
    // An unfinished bounded map job remains future state; never wait on it here.
    const_cast<Transportation*>(this)->finishMapJob(false);
    SaveFile::reserveSnapshotBytes(sizeof(Transportation)+boats_.size()*sizeof(Boat)+maps_.size()*sizeof(ExplorationMap)+waypoints_.size()*(sizeof(Waypoint)+24));
    auto copy=std::make_shared<Transportation>(spawn_);
    copy->nextBoat_=nextBoat_;copy->nextMap_=nextMap_;copy->rider_=rider_;copy->selectedWaypoint=selectedWaypoint;
    copy->boats_=boats_;copy->maps_=maps_;copy->waypoints_=waypoints_;
    std::shared_ptr<const Transportation> snapshot=std::move(copy);
    return [snapshot,path,seed]{return snapshot->save(path,seed);};
}

std::function<bool(std::ostream&)> Automation::captureWrite() const {
    using Record=std::pair<LogicPosition,Node>;
    if(nodes_.size()>100000)throw std::length_error("Automation snapshot exceeds device limit");
    SaveFile::reserveSnapshotBytes(nodes_.size()*sizeof(Record)+sizeof(ranges_)+4096);
    auto copy=std::make_shared<const std::vector<Record>>(nodes_.begin(),nodes_.end());
    const auto ranges=ranges_;const auto tick=stats_.ticks;
    return [copy,ranges,tick](std::ostream& out) {
        for(const auto& e:*copy){const auto p=e.first;const auto& n=e.second;
            if(!SaveValidation::position(glm::vec3(p.x,p.y,p.z))||!automationDevice(n.type)||n.type!=blockType(n.type)||
               n.remaining>10||n.due>tick+10||n.target>15||n.input>15||!validBlockState(n.observed))return false;}
        const auto put=[&](const auto& v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));};
        out.write("AUT1",4);put(std::uint32_t(copy->size()));
        for(const auto& e:*copy){const auto p=e.first;const auto& n=e.second;
            unsigned remaining=n.remaining;
            if(n.due && tick-n.timerAt<ranges.size())
                for(auto t=n.timerAt+1;t<=tick&&remaining;++t){const auto& r=ranges[t%ranges.size()];if(r.tick!=t)continue;
                    const auto dx=std::int64_t(World::floorDiv(p.x,16))-r.x,dz=std::int64_t(World::floorDiv(p.z,16))-r.z;
                    if(!r.valid||dx*dx+dz*dz<=std::int64_t(r.radius)*r.radius)--remaining;}
            put(p.x);put(p.y);put(p.z);put(std::uint32_t(n.type));put(std::uint16_t(remaining));put(n.target);put(n.input);put(std::uint32_t(n.observed));
        }
        return bool(out);
    };
}

std::future<bool> Game::captureCheckpointAsync(std::shared_ptr<SaveFile::CommitStatus> status) {
    const auto start=std::chrono::steady_clock::now();
    world_->adventure.observeInventory(*inventory_);
    SaveFile::SnapshotBudget budget;
    std::vector<SaveFile::SnapshotComponent> components;components.reserve(7);
    components.push_back(world_->captureSaveWorld(worldSavePath("voxel_world.vxw"),player_->position()));
    components.push_back(transportation_->captureSave(worldSavePath("voxel_travel.vtn"),seed_));
    components.push_back(inventory_->captureSave(worldSavePath("voxel_inventory.vxi"),seed_));
    components.push_back(player_->captureSave(worldSavePath("voxel_player.vps"),seed_));
    components.push_back(survival_->captureSave(worldSavePath("voxel_mobs.vxm"),seed_));
    // These two tiny metadata records are captured here; bulk binary encoding,
    // integrity verification and all staging run under the worker transaction.
    std::ostringstream metadata;
    metadata<<seed_<<'\n'<<(spectatorMode_?"spectator":creativeMode_?"creative":"survival")<<'\n'
        <<std::setprecision(std::numeric_limits<float>::max_digits10)
        <<std::fmod(std::max(0.f,timing_.worldTime),WorldTime::CycleSeconds)<<'\n';
    if(weather_)weather_->write(metadata);
    metadata<<"TIME_CYCLE1 "<<WorldTime::CycleSeconds<<'\n';
    auto text=metadata.str();auto path=worldSavePath("world_seed.txt");
    SaveFile::reserveSnapshotBytes(text.size()+4096);
    components.push_back([text=std::move(text),path]{return SaveFile::write(path,std::ios::out,[&](std::ofstream& out){out<<text;return bool(out);});});
    if(!activeWorld_.directory.empty()) {
        auto info=activeWorld_;info.lastPlayed=std::int64_t(std::time(nullptr));
        SaveFile::reserveSnapshotBytes(sizeof(info)+4096);
        components.push_back([info=std::move(info)]{return WorldLibrary::writeMetadata(info);});
    }
    status->snapshotBytes=budget.bytes();
    status->captureMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return SaveFile::commitSnapshotAsync(worldSavePath("save.checkpoint"),std::move(components),std::move(status));
}
