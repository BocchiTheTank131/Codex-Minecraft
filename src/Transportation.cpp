#include "Transportation.h"
#include "World.h"
#include "Player.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>

namespace {
bool overlaps(const glm::vec3& a,const glm::vec3& b,const glm::vec3& c,const glm::vec3& d){return a.x<d.x && b.x>c.x && a.y<d.y && b.y>c.y && a.z<d.z && b.z>c.z;}
bool clearBox(const World& w,glm::vec3 low,glm::vec3 high){
    for(int z=int(std::floor(low.z));z<=int(std::floor(high.z-.001f));++z)
    for(int x=int(std::floor(low.x));x<=int(std::floor(high.x-.001f));++x){
        if(!w.hasLoadedChunkAt(x,z)) return false;
        for(int y=int(std::floor(low.y));y<=int(std::floor(high.y-.001f));++y){
            glm::vec3 a,b;
            if(isLava(w.getBlock(x,y,z))) return false;
            if(w.blockCollisionBounds({x,y,z},a,b) && overlaps(low,high,a,b)) return false;
        }
    }return true;
}
constexpr float HullX=.67f,HullZ=.64f;
bool hullOverlaps(glm::vec3 p,float heading,glm::vec3 low,glm::vec3 high){
    if(p.y>=high.y || p.y+.54f<=low.y)return false;
    if(p.x+1<=low.x || p.x-1>=high.x || p.z+1<=low.z || p.z-1>=high.z)return false;
    const float c=std::cos(heading),s=std::sin(heading),ac=std::abs(c),as=std::abs(s);
    const auto center=(low+high)*.5f,half=(high-low)*.5f;const float dx=center.x-p.x,dz=center.z-p.z;
    return std::abs(dx)<half.x+ac*HullX+as*HullZ && std::abs(dz)<half.z+as*HullX+ac*HullZ &&
        std::abs(dx*c+dz*s)<HullX+half.x*ac+half.z*as && std::abs(-dx*s+dz*c)<HullZ+half.x*as+half.z*ac;
}
bool hullsOverlap(glm::vec3 p,float angle,const Boat& other){
    if(std::abs(p.y-other.position.y)>=.54f || std::abs(p.x-other.position.x)>=1.86f || std::abs(p.z-other.position.z)>=1.86f)return false;
    const glm::vec2 d(other.position.x-p.x,other.position.z-p.z);
    const glm::vec2 f(std::cos(angle),std::sin(angle)),r(-f.y,f.x),g(std::cos(other.heading),std::sin(other.heading)),t(-g.y,g.x);
    for(const auto axis:{f,r,g,t})if(std::abs(glm::dot(d,axis))>=HullX*(std::abs(glm::dot(f,axis))+std::abs(glm::dot(g,axis)))+HullZ*(std::abs(glm::dot(r,axis))+std::abs(glm::dot(t,axis))))return false;
    return true;
}
bool hullClear(const World& w,glm::vec3 p,bool rider,float heading){
    const float c=std::abs(std::cos(heading)),s=std::abs(std::sin(heading)),ex=c*HullX+s*HullZ,ez=s*HullX+c*HullZ;
    for(int z=int(std::floor(p.z-ez));z<=int(std::floor(p.z+ez));++z)for(int x=int(std::floor(p.x-ex));x<=int(std::floor(p.x+ex));++x){
        if(!w.hasLoadedChunkAt(x,z))return false;
        for(int y=int(std::floor(p.y));y<=int(std::floor(p.y+.54f));++y){glm::vec3 low,high;
            if(isLava(w.getBlock(x,y,z)) && hullOverlaps(p,heading,glm::vec3(x,y,z),glm::vec3(x+1,y+1,z+1)))return false;
            if(w.blockCollisionBounds({x,y,z},low,high)&&hullOverlaps(p,heading,low,high))return false;
        }
    }
    return !rider||clearBox(w,p+glm::vec3(-.3f,.25f,-.3f),p+glm::vec3(.3f,2.05f,.3f));
}
float surface(const World& w,glm::vec3 p){
    std::array<float,9> heights{};int samples=0;
    for(float x:{-.48f,0.f,.48f})for(float z:{-.48f,0.f,.48f}){
        const int wx=int(std::floor(p.x+x)),wz=int(std::floor(p.z+z));
        if(!w.hasLoadedChunkAt(wx,wz))continue;
        float top=-10000;
        for(int y=int(std::floor(p.y))+2;y>=int(std::floor(p.y))-2;--y){
            const auto b=w.getBlock(wx,y,wz);
            if(isWater(b) && !isWater(w.getBlock(wx,y+1,wz))){top=float(y)+waterHeight(b);break;}
        }
        if(top>-9999)heights[samples++]=top;
    }if(samples<3)return -10000;
    std::sort(heights.begin(),heights.begin()+samples);return heights[samples/2];
}
bool boatHit(const Boat& b,glm::vec3 o,glm::vec3 d,float reach,float* distance=nullptr){
    const auto relative=o-b.position;const float cs=std::cos(b.heading),sn=std::sin(b.heading);
    o={relative.x*cs+relative.z*sn,relative.y,-relative.x*sn+relative.z*cs};d={d.x*cs+d.z*sn,d.y,-d.x*sn+d.z*cs};
    glm::vec3 a(-HullX,0,-HullZ),c(HullX,.55f,HullZ);
    float low=0,high=reach;
    for(int i=0;i<3;++i){if(std::abs(d[i])<1e-6f){if(o[i]<a[i]||o[i]>c[i])return false;}else{float t=(a[i]-o[i])/d[i],u=(c[i]-o[i])/d[i];if(t>u)std::swap(t,u);low=std::max(low,t);high=std::min(high,u);if(low>high)return false;}}if(distance)*distance=low;return true;
}
}
bool Transportation::placeBoat(World& w,Item item,const glm::vec3& hit,float heading){
    if(!isBoatItem(item)||boats_.size()>=1024) return false;
    glm::vec3 p=hit;const float water=surface(w,p);
    if(water<-9999)return false;p.y=water-.23f;
    if(!hullClear(w,p,false,heading))return false;
    for(const auto& b:boats_)if(hullsOverlap(p,heading,b))return false;
    boats_.push_back({nextBoat_++,item,p,{},heading,0});return true;
}
bool Transportation::useBoat(Player& p,const World& w,const glm::vec3& o,const glm::vec3& d,float reach){
    if(mounted() || p.isSpectator() || p.isDead())return false;
    Boat* closest=nullptr;float distance=reach+2;
    for(auto& b:boats_){float along=0;if(boatHit(b,o,d,reach,&along) && along<distance && hullClear(w,b.position,true,b.heading)){closest=&b;distance=along;}}
    if(!closest)return false;rider_=closest->id;riderRevision_=p.positionRevision();p.setRiding(true);p.setRidePosition(closest->position+glm::vec3(0,.25f,0),closest->velocity);return true;
}
bool Transportation::dismount(Player& p,const World& w){
    if(!mounted())return true;
    const auto it=std::find_if(boats_.begin(),boats_.end(),[&](const Boat& b){return b.id==rider_;});
    if(it==boats_.end()){rider_=0;p.setRiding(false);return true;}
    for(float radius:{1.6f,2.3f,3.f})for(int i=0;i<16;++i){const float a=it->heading+i*6.2831853f/16;
        glm::vec3 q=it->position+glm::vec3(std::cos(a)*radius,0,std::sin(a)*radius);
        for(int dy=2;dy>=-2;--dy){q.y=std::floor(it->position.y)+dy;
            const int x=int(std::floor(q.x)),y=int(std::floor(q.y)),z=int(std::floor(q.z));
            glm::vec3 low,high;const bool ground=w.blockCollisionBounds({x,y-1,z},low,high);
            const bool water=isWater(w.getBlock(x,y-1,z))||isWater(w.getBlock(x,y,z));
            if(!ground&&!water)continue;
            if(water){const float top=surface(w,q);if(top<-9999)continue;q.y=top+.01f;}
            else q.y=high.y+.01f;
            if(!clearBox(w,q+glm::vec3(-.3f,0,-.3f),q+glm::vec3(.3f,1.8f,.3f)))continue;
            bool blocked=false;for(const auto& b:boats_)if(hullOverlaps(b.position,b.heading,q+glm::vec3(-.3f,0,-.3f),q+glm::vec3(.3f,1.8f,.3f)))blocked=true;
            if(blocked)continue;
            rider_=0;p.setRiding(false);p.setRidePosition(q,{});return true;
        }
    }return false; // Keep the seat until a real safe exit exists; never place the player in a block.
}
bool Transportation::breakBoat(Player& p,Inventory&,SurvivalWorld& s,const World& w,float reach){
    if(mounted())return false;
    auto nearest=boats_.end();float distance=reach+1;
    for(auto it=boats_.begin();it!=boats_.end();++it){float along=0;if(boatHit(*it,p.cameraPosition(),p.lookDirection(),reach,&along)&&along<distance){nearest=it;distance=along;}}
    if(nearest==boats_.end())return false;
    RayHit hit;if(w.raycast(p.cameraPosition(),p.lookDirection(),distance,hit)&&hit.distance+.01f<distance)return false;
    s.spawnDrop(nearest->position+glm::vec3(0,.4f,0),nearest->item);boats_.erase(nearest);return true;
}
void Transportation::update(float dt,const PlayerInput& input,World& w,Player& p,const SurvivalWorld& mobs){
    const auto started=std::chrono::steady_clock::now();
    boatCollisionCandidates=0;boatSteps=0;revealTimer_=std::min(2.f,revealTimer_+std::max(0.f,dt));
    finishMapJob(false);
    if(mounted() && (p.isDead()||p.positionRevision()!=riderRevision_||p.isSpectator() || glm::distance(p.position(),[&]{for(const auto& b:boats_)if(b.id==rider_)return b.position;return p.position();}())>4)) {rider_=0;p.setRiding(false);}
    if(mounted())p.setRiding(true);
    if(mounted() && input.enabled && input.sneak && !sneakWasDown_)dismount(p,w);
    sneakWasDown_=input.enabled&&input.sneak;
    accumulator_=std::min(accumulator_+std::max(0.f,dt),1.f);
    // One sorted grid per frame. Candidate envelopes include both hulls' maximum
    // travel during this catch-up interval, so crossing a cell cannot miss contact.
    hullGrid_.clear();activeBoats_.clear();hullCandidates_.resize(boats_.size());
    float maximumSpeed=mounted()&&input.enabled&&(input.moveForward||input.moveBackward)?8.6f:0.f;
    for(std::size_t i=0;i<boats_.size();++i){auto& b=boats_[i];
        b.velocity=glm::clamp(b.velocity,glm::vec3(-32,-18,-32),glm::vec3(32,4,32));
        hullGrid_.push_back({int(std::floor(b.position.z/8)),int(std::floor(b.position.x/8)),i});
        maximumSpeed=std::max(maximumSpeed,glm::length(glm::vec2(b.velocity.x,b.velocity.z)));
        if(glm::distance(b.position,p.position())<=96 && w.hasLoadedChunkAt(int(std::floor(b.position.x)),int(std::floor(b.position.z))))activeBoats_.push_back(i);
    }
    std::sort(hullGrid_.begin(),hullGrid_.end(),[](const HullCell& a,const HullCell& b){return a.z<b.z||(a.z==b.z&&a.x<b.x);});
    const float envelope=2.f+2*maximumSpeed*accumulator_;
    for(auto i:activeBoats_){auto& candidates=hullCandidates_[i];candidates.clear();const auto center=boats_[i].position;
        const int x0=int(std::floor((center.x-envelope)/8)),x1=int(std::floor((center.x+envelope)/8));
        for(int z=int(std::floor((center.z-envelope)/8));z<=int(std::floor((center.z+envelope)/8));++z){
            auto it=std::lower_bound(hullGrid_.begin(),hullGrid_.end(),HullCell{z,x0,0},[](const HullCell& a,const HullCell& b){return a.z<b.z||(a.z==b.z&&a.x<b.x);});
            for(;it!=hullGrid_.end()&&it->z==z&&it->x<=x1;++it)if(it->index!=i)candidates.push_back(it->index);
        }
    }
    // Only nearby loaded hulls simulate. Unloaded columns are collision barriers, never empty space.
    if(!boats_.empty()){
        mobs.vehicleObstacles(obstacles_,p.position(),100);
        if(!p.isSpectator()&&!p.isDead()){RenderCuboid body{p.position()+glm::vec3(0,Player::Height*.5f,0),{Player::Width,Player::Height,Player::Width},{1,1,1},{}};body.woodTexture=-2;obstacles_.push_back(body);}
    }
    // Normal frames retain 120 Hz. A spike uses at most 32 stable integration
    // steps; swept axis contacts below bound spatial travel independently of dt.
    const float h=std::max(1.f/120.f,accumulator_/32.f);
    while(accumulator_+1e-7f>=h && boatSteps<32){accumulator_=std::max(0.f,accumulator_-h);++boatSteps;
        for(const auto index:activeBoats_){auto& b=boats_[index];
            const bool riding=b.id==rider_;const float top=surface(w,b.position);const bool afloat=top>-9999;
            const float steer=riding&&input.enabled?float(input.moveRight)-float(input.moveLeft):0;
            b.angularVelocity+=(steer*1.7f-b.angularVelocity)*std::min(1.f,h*8);
            const float turn=std::remainder(b.heading+b.angularVelocity*h,6.2831853f);
            const bool turning=std::abs(turn-b.heading)>1e-7f;
            bool turnClear=!turning||hullClear(w,b.position,riding,turn);
            if(turning)for(const auto candidate:hullCandidates_[index]){++boatCollisionCandidates;const auto& other=boats_[candidate];if(hullsOverlap(b.position,turn,other))turnClear=false;}
            if(turning)for(const auto& mob:obstacles_)if(!(riding&&mob.woodTexture==-2) && hullOverlaps(b.position,turn,mob.center-mob.size*.5f,mob.center+mob.size*.5f) && !hullOverlaps(b.position,b.heading,mob.center-mob.size*.5f,mob.center+mob.size*.5f))turnClear=false;
            if(turnClear)b.heading=turn;else b.angularVelocity=0;
            const float throttle=riding&&input.enabled?float(input.moveForward)-float(input.moveBackward):0;
            const glm::vec3 forward(std::cos(b.heading),0,std::sin(b.heading));
            if(afloat)b.velocity+=forward*throttle*(throttle<0?5.f:9.f)*h;
            const float drag=afloat?1.05f:6.f;b.velocity.x*=std::exp(-drag*h);b.velocity.z*=std::exp(-drag*h);
            if(afloat)b.velocity.y+=((top-.23f-b.position.y)*35.f-b.velocity.y*11.f)*h;else b.velocity.y-=22*h;
            b.velocity.y=std::clamp(b.velocity.y,-18.f,4.f);
            for(int axis:{0,2,1}){
                const float travel=b.velocity[axis]*h;if(std::abs(travel)<1e-7f)continue;
                const int segments=std::max(1,int(std::ceil(std::abs(travel)/.15f)));
                const auto blocked=[&](glm::vec3 q){
                    if(!hullClear(w,q,riding,b.heading))return true;
                    for(const auto candidate:hullCandidates_[index]){++boatCollisionCandidates;if(hullsOverlap(q,b.heading,boats_[candidate]))return true;}
                    for(const auto& mob:obstacles_)if(!(riding&&mob.woodTexture==-2) && glm::distance(q,mob.center)<=glm::distance(b.position,mob.center)+.00001f && hullOverlaps(q,b.heading,mob.center-mob.size*.5f,mob.center+mob.size*.5f))return true;
                    return false;
                };
                for(int n=0;n<segments;++n){auto q=b.position;q[axis]+=travel/segments;
                    if(blocked(q)){
                        // Advance up to contact instead of leaving a full-step shore gap.
                        float lo=0,hi=1;for(int k=0;k<6;++k){const float mid=(lo+hi)*.5f;auto test=b.position;test[axis]+=(q[axis]-b.position[axis])*mid;if(blocked(test))hi=mid;else lo=mid;}
                        b.position[axis]+=(q[axis]-b.position[axis])*lo;b.velocity[axis]=0;break;
                    }b.position=q;
                }
            }
            if(riding)p.setRidePosition(b.position+glm::vec3(0,.25f,0),b.velocity);
        }
    }
    boatMilliseconds=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-started).count();
    const auto d=spawn_-p.position(),f=p.lookDirection();
    const float desired=std::atan2(d.z,d.x)-std::atan2(f.z,f.x);
    compassAngle+=std::remainder(desired-compassAngle,6.2831853f)*(1-std::exp(-10*dt));
}
void Transportation::appendModels(std::vector<RenderCuboid>& out,const glm::vec3& camera,float distance) const{
    const glm::vec3 colors[]={{.57f,.36f,.17f},{.79f,.72f,.48f},{.34f,.23f,.13f},{.65f,.43f,.27f},{.71f,.34f,.16f},{.25f,.15f,.09f}};
    for(const auto& b:boats_)if(glm::distance(camera,b.position)<distance){
        const auto color=colors[int(b.item)-int(Item::OakBoat)];
        auto part=[&](glm::vec3 local,glm::vec3 size,glm::vec3 c){const float a=b.heading;const auto pos=b.position+glm::vec3(local.x*std::cos(a)-local.z*std::sin(a),local.y,local.x*std::sin(a)+local.z*std::cos(a));const float shade=c.r/std::max(.01f,color.r);RenderCuboid box{pos,size,glm::vec3(shade),{}};box.yaw=a;box.woodTexture=int(b.item)-int(Item::OakBoat);out.push_back(box);};
        part({0,.07f,0},{1.32f,.14f,1.15f},color*.82f);
        for(float side:{-.57f,.57f})part({0,.29f,side},{1.3f,.4f,.12f},color);
        for(float end:{-.6f,.6f})part({end,.27f,0},{.12f,.35f,1.15f},color*.9f);
        part({.05f,.3f,0},{.22f,.12f,1.02f},color*1.12f);
        for(float side:{-.76f,.76f})part({0,.27f,side},{.65f,.065f,.09f},color*1.15f);

    }
}

bool Transportation::intersectsBox(glm::vec3 low,glm::vec3 high) const{
    for(const auto& b:boats_)if(hullOverlaps(b.position,b.heading,low,high))return true;
    return false;
}

float Transportation::landingHeight(glm::vec3 low,glm::vec3 high,float previousFeet) const{
    float result=-std::numeric_limits<float>::infinity();
    for(const auto& b:boats_){const float top=b.position.y+.54f;if(previousFeet<top-.19f||previousFeet>top+.5f)continue;
        low.y=b.position.y;high.y=top;if(hullOverlaps(b.position,b.heading,low,high))result=std::max(result,top);
    }return result;
}
