#include "Transportation.h"
#include "World.h"
#include "Player.h"
#include "SaveFile.h"
#include "Persistence.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <cstring>
#include <unordered_set>

namespace {
template<class T>bool wr(std::ofstream& f,const T& t){f.write(reinterpret_cast<const char*>(&t),sizeof(t));return bool(f);}
template<class T>bool rd(std::ifstream& f,T& t){f.read(reinterpret_cast<char*>(&t),sizeof(t));return bool(f);}
bool valid(glm::vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<30000000&&std::abs(p.z)<30000000&&std::abs(p.y)<10000;}
std::string cleanName(std::string name){name.erase(std::remove_if(name.begin(),name.end(),[](unsigned char c){return c<32||c>126;}),name.end());if(name.size()>24)name.resize(24);return name;}
}
Transportation::~Transportation(){finishMapJob(true);}
const ExplorationMap* Transportation::map(std::uint32_t id) const{for(const auto& m:maps_)if(m.id==id)return &m;return nullptr;}
std::uint32_t Transportation::createMap(const glm::vec3& p,int scale){
    if(maps_.size()>=128 || !valid(p))return 0;
    ExplorationMap m;m.id=nextMap_++;m.scale=std::clamp(scale,0,4);const int span=128<<m.scale;
    m.originX=int(std::floor(p.x/32))*32-span/2;m.originZ=int(std::floor(p.z/32))*32-span/2;
    maps_.push_back(std::move(m));return maps_.back().id;
}
std::uint32_t Transportation::zoomMap(std::uint32_t id){
    finishMapJob(true);const auto* source=map(id);if(!source||source->scale>=4||maps_.size()>=128)return 0;
    const ExplorationMap old=*source;const int oldStep=1<<old.scale;
    const auto result=createMap({float(old.originX+64*oldStep),0,float(old.originZ+64*oldStep)},old.scale+1);
    auto& dest=maps_.back();const int step=oldStep*2;
    // Zoom preserves only already surveyed pixels. It never reveals new territory.
    for(int y=0;y<128;++y)for(int x=0;x<128;++x)if(old.pixels[y*128+x]){
        const int dx=(old.originX+x*oldStep-dest.originX)/step,dz=(old.originZ+y*oldStep-dest.originZ)/step;
        if(dx>=0&&dz>=0&&dx<128&&dz<128)dest.pixels[dz*128+dx]=old.pixels[y*128+x];
    }++dest.revision;return result;
}
void Transportation::finishMapJob(bool wait){
    if(!mapJob_.valid()||(!wait&&mapJob_.wait_for(std::chrono::seconds(0))!=std::future_status::ready))return;
    const auto result=mapJob_.get();for(auto& m:maps_)if(m.id==result.id){for(auto p:result.pixels)m.pixels[p.first]=p.second;++m.revision;break;}
}
void Transportation::reveal(const World& w,const Player& player,std::uint32_t id){
    const auto start=std::chrono::steady_clock::now();finishMapJob(false);
    const auto* m=map(id);if(!m||mapJob_.valid())return;
    struct Sample{int index,height;glm::vec3 color;};std::vector<Sample> samples;const std::size_t budget=m->scale?48:96;samples.reserve(budget);
    const int step=1<<m->scale;
    const int px=int(std::floor((player.position().x-m->originX)/step)),pz=int(std::floor((player.position().z-m->originZ)/step));
    const int radius=std::max(2,48/step);
    for(int z=std::max(0,pz-radius);z<=std::min(127,pz+radius)&&samples.size()<budget;++z)
    for(int x=std::max(0,px-radius);x<=std::min(127,px+radius)&&samples.size()<budget;++x){
        const int index=z*128+x;if(m->pixels[index]||(x-px)*(x-px)+(z-pz)*(z-pz)>radius*radius)continue;
        const int wx=m->originX+x*step,wz=m->originZ+z*step;
        if(!w.hasLoadedChunkAt(wx,wz))continue;
        const int y=int(std::floor(w.precipitationHeight(wx,wz)-.01f));
        if(player.position().y<y-4)continue; // A cave does not survey the unseen surface overhead.
        const auto b=w.getBlock(wx,y,wz);const auto c=w.biomeClimateAt(wx,wz,float(y));
        glm::vec3 color=climateGrassColor(c);
        if(isWater(b))color=climateWaterColor(c);
        else if(isLeaf(b))color=climateFoliageColor(c)*.8f;
        else if(b==Block::Sand||b==Block::Sandstone)color={.82f,.75f,.47f};
        else if(b==Block::SnowBlock||b==Block::Snow||b==Block::Ice||(b>=Block::Snow2&&b<=Block::Snow8))color={.86f,.92f,.95f};
        else if(b==Block::Stone||b==Block::Gravel||b==Block::Deepslate)color={.46f,.47f,.49f};
        samples.push_back({index,y,color});
    }
    if(!samples.empty())mapJob_=std::async(std::launch::async,[id,samples=std::move(samples)]{
        MapResult r;r.id=id;r.pixels.reserve(samples.size());
        for(const auto& s:samples){const float shade=std::clamp(.82f+(s.height-64)*.002f,.65f,1.13f);const auto c=glm::clamp(s.color*shade,glm::vec3(0),glm::vec3(1));
            r.pixels.push_back({s.index,0xff000000U|std::uint32_t(c.r*255)|(std::uint32_t(c.g*255)<<8)|(std::uint32_t(c.b*255)<<16)});}
        return r;
    });
    mapMilliseconds=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
    mapTotalMilliseconds+=mapMilliseconds;mapWorstMilliseconds=std::max(mapWorstMilliseconds,double(mapMilliseconds));++mapBatches;
}
bool Transportation::addWaypoint(std::string name,const glm::vec3& p){name=cleanName(name);if(waypoints_.size()>=64||name.empty()||!valid(p))return false;waypoints_.push_back({name,p});selectedWaypoint=int(waypoints_.size())-1;return true;}
bool Transportation::renameWaypoint(int i,std::string name){name=cleanName(name);if(i<0||i>=int(waypoints_.size())||name.empty())return false;waypoints_[i].name=name;return true;}
void Transportation::removeWaypoint(int i){if(i>=0&&i<int(waypoints_.size())){waypoints_.erase(waypoints_.begin()+i);selectedWaypoint=waypoints_.empty()?-1:std::min(i,int(waypoints_.size())-1);}}
glm::vec3 Transportation::target() const{return selectedWaypoint>=0&&selectedWaypoint<int(waypoints_.size())?waypoints_[selectedWaypoint].position:spawn_;}
bool Transportation::save(const std::string& path,std::uint32_t seed) const{
    if(!Persistence::enabled())return true;
    const_cast<Transportation*>(this)->finishMapJob(true);
    return SaveFile::write(path,std::ios::binary,[&](std::ofstream& f){
        f.write("VXTRAV1",8);wr(f,seed);wr(f,spawn_);wr(f,nextBoat_);wr(f,nextMap_);wr(f,selectedWaypoint);wr(f,rider_);
        const auto nb=std::uint32_t(boats_.size());wr(f,nb);
        for(const auto& b:boats_){wr(f,b.id);wr(f,b.item);wr(f,b.position);wr(f,b.velocity);wr(f,b.heading);}
        const auto nm=std::uint32_t(maps_.size());wr(f,nm);
        for(const auto& m:maps_){wr(f,m.id);wr(f,m.originX);wr(f,m.originZ);wr(f,m.scale);
            // Sparse surveyed pixels; switch to a raw plane before sparse encoding exceeds 64 KiB.
            std::uint32_t n=0;for(auto color:m.pixels)if(color)++n;
            if(n*6>65536){wr(f,~0U);f.write(reinterpret_cast<const char*>(m.pixels.data()),65536);}
            else {wr(f,n);for(int i=0;i<128*128;++i)if(m.pixels[i]){wr(f,std::uint16_t(i));wr(f,m.pixels[i]);}}
        }
        wr(f,std::uint32_t(waypoints_.size()));for(const auto& wp:waypoints_){wr(f,std::uint8_t(wp.name.size()));f.write(wp.name.data(),wp.name.size());wr(f,wp.position);}return bool(f);
    });
}
bool Transportation::load(const std::string& path,std::uint32_t seed){
    if(!Persistence::enabled())return true;
    std::ifstream f(path,std::ios::binary);if(!f)return false;
    char magic[8]{};f.read(magic,8);std::uint32_t stored,n;Transportation temp(spawn_);
    if(std::memcmp(magic,"VXTRAV1",8)||!rd(f,stored)||stored!=seed||!rd(f,temp.spawn_)||!valid(temp.spawn_)||!rd(f,temp.nextBoat_)||!rd(f,temp.nextMap_)||!rd(f,temp.selectedWaypoint)||!rd(f,temp.rider_))return false;
    if(!rd(f,n)||n>1024)return false;
    std::unordered_set<std::uint32_t> ids;
    for(unsigned i=0;i<n;++i){Boat b;if(!rd(f,b.id)||!rd(f,b.item)||!isBoatItem(b.item)||!rd(f,b.position)||!valid(b.position)||!rd(f,b.velocity)||!valid(b.velocity)||!rd(f,b.heading)||!std::isfinite(b.heading)||!b.id||!ids.insert(b.id).second||b.id>=temp.nextBoat_)return false;temp.boats_.push_back(b);}
    if(!rd(f,n)||n>128)return false;ids.clear();
    for(unsigned i=0;i<n;++i){ExplorationMap m;std::uint32_t count;if(!rd(f,m.id)||!m.id||m.id>=temp.nextMap_||!ids.insert(m.id).second||!rd(f,m.originX)||!rd(f,m.originZ)||std::abs(double(m.originX))>30002048||std::abs(double(m.originZ))>30002048||!rd(f,m.scale)||m.scale<0||m.scale>4||!rd(f,count)||(count!=~0U && count>16384))return false;
        if(count==~0U){f.read(reinterpret_cast<char*>(m.pixels.data()),65536);if(!f)return false;}
        else for(unsigned k=0;k<count;++k){std::uint16_t index;std::uint32_t color;if(!rd(f,index)||index>=16384||!rd(f,color)||m.pixels[index])return false;m.pixels[index]=color;}temp.maps_.push_back(m);}
    if(!rd(f,n)||n>64)return false;
    for(unsigned i=0;i<n;++i){Waypoint wp;std::uint8_t length;if(!rd(f,length)||!length||length>24)return false;wp.name.resize(length);f.read(wp.name.data(),length);if(!rd(f,wp.position)||!valid(wp.position)||cleanName(wp.name)!=wp.name)return false;temp.waypoints_.push_back(wp);}
    if(temp.selectedWaypoint< -1||temp.selectedWaypoint>=int(temp.waypoints_.size())||f.peek()!=std::char_traits<char>::eof())return false;
    if(temp.rider_ && std::none_of(temp.boats_.begin(),temp.boats_.end(),[&](const auto& b){return b.id==temp.rider_;}))return false;
    finishMapJob(true);spawn_=temp.spawn_;nextBoat_=temp.nextBoat_;nextMap_=temp.nextMap_;selectedWaypoint=temp.selectedWaypoint;
    boats_=std::move(temp.boats_);maps_=std::move(temp.maps_);waypoints_=std::move(temp.waypoints_);rider_=temp.rider_;riderRevision_=0;return true;
}
