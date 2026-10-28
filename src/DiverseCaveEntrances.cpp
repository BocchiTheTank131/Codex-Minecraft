#include "TerrainExploration.h"
#include "TerrainDensity.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace TerrainExploration {
namespace {
int floorCell(int value,int span) {return int(std::floor(double(value)/span));}
struct Plan {bool valid=false; CaveEntrance cave{};};
}
std::vector<CaveEntrance> diverseCaveEntrances(int cx,int cz,std::uint32_t seed,
    const std::function<SurfaceColumn(int,int)>& terrain,
    const std::function<bool(int,int,int)>& cavity,
    const std::function<bool(int,int)>& reserved,unsigned generationVersion) {
    // Per-worker, bounded cache. Failed plans are cached as well, so dense
    // column sampling does not repeat cavity/reservation searches.
    thread_local std::uint32_t cacheSeed=0,cacheVersion=0;
    thread_local std::unordered_map<std::uint64_t,Plan> cache;
    if(cacheSeed!=seed || cacheVersion!=generationVersion || cache.size()>512) {cache.clear();cacheSeed=seed;cacheVersion=generationVersion;}
    const int span=generationVersion>=23?52:80;
    constexpr int reach=88;
    const int ox=cx*16,oz=cz*16;
    std::vector<CaveEntrance> result;
    for(int rz=floorCell(oz-reach,span);rz<=floorCell(oz+15+reach,span);++rz)
    for(int rx=floorCell(ox-reach,span);rx<=floorCell(ox+15+reach,span);++rx) {
        const auto h=TerrainDensity::hash(rx,0,rz,seed^0xca7eU);
        if(h%3U==2) continue;
        const auto key=(std::uint64_t(std::uint32_t(rx))<<32)|std::uint32_t(rz);
        auto found=cache.find(key);
        if(found==cache.end()) {
            Plan plan;
            for(int candidate=0;candidate<3 && !plan.valid;++candidate) {
                const auto roll=TerrainDensity::hash(rx,candidate,rz,seed^0x22caU);
                const unsigned jitter=generationVersion>=23?32U:60U;
                const int x=rx*span+10+int((roll>>5)%jitter),z=rz*span+10+int((roll>>12)%jitter);
                const auto surface=terrain(x,z);
                if(surface.height<70 || surface.height>240 || surface.river>.01f ||
                   surface.ravine>.02f || reserved(x,z)) continue;
                constexpr int directions[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
                for(int attempt=0;attempt<4 && !plan.valid;++attempt) {
                    const auto& d=directions[((roll>>19)+attempt)%4U];
                    const int length=64+int((roll>>22)%9U);
                    const int tx=x+d[0]*length+d[1]*7,tz=z+d[1]*length-d[0]*7;
                    const int localHeight=terrain(tx,tz).height;
                    const float radius=2.f+float((roll>>24)%4U)*.4f;
                    for(int depth=20;depth<=36;depth+=2) {
                        const int y=std::min(surface.height-depth,localHeight-18);
                        if(y<8 || surface.height-y>length*.55f || cavity(tx,y-2,tz) || !cavity(tx,y,tz) ||
                           !cavity(tx,y+2,tz) || !cavity(tx+2,y+1,tz) || !cavity(tx-2,y+1,tz)) continue;
                        // Reject missing native cavities before paying for full
                        // route checks. Selection and output stay identical.
                        bool protectedRoute=false,unsupported=false;
                        for(int step=0;step<=12 && !protectedRoute && !unsupported;++step) {
                            const int px=x+(tx-x)*step/12,pz=z+(tz-z)*step/12;
                            for(int dz=-5;dz<=5;dz+=5)for(int dx=-5;dx<=5;dx+=5) {
                                const auto s=terrain(px+dx,pz+dz);
                                protectedRoute |= s.height<68 || s.river>.01f ||
                                    s.ravine>.02f || reserved(px+dx,pz+dz);
                            }
                            // Existing cavern air below the bore would turn a
                            // gentle slope into a lethal drop. Find a supported
                            // approach rather than constructing artificial bridges.
                            const float centre=surface.height+1.5f+(y-surface.height-.5f)*step/12.f;
                            if(!protectedRoute)unsupported=cavity(px,int(std::floor(centre-radius-1.f)),pz);
                        }
                        if(protectedRoute) break;
                        if(unsupported)continue;
                        if(generationVersion>=23) {
                            // Coarse checks can miss a one-column native pit:
                            // the player falls below the sloping bore and meets
                            // a two/three-block lip on its far side. Check the
                            // actual walking ribbon at block intervals, only
                            // after the inexpensive historical route rejection.
                            for(int step=0;step<=length && !unsupported;++step) {
                                const float t=float(step)/float(length);
                                const int px=int(std::floor(float(x)+.5f+float(tx-x)*t));
                                const int pz=int(std::floor(float(z)+.5f+float(tz-z)*t));
                                const float centre=surface.height+1.5f+(y-surface.height-.5f)*t;
                                const int floorY=int(std::floor(centre-radius-1.f));
                                for(int side=-1;side<=1;++side)
                                    unsupported|=cavity(px+d[1]*side,floorY,pz-d[0]*side);
                            }
                            if(unsupported)continue;
                        }
                        plan.cave={{float(x)+.5f,float(surface.height)+1.5f,float(z)+.5f},
                            {float(x+tx)*.5f+.5f,(float(surface.height)+float(y))*.5f+1.f,float(z+tz)*.5f+.5f},
                            {float(tx)+.5f,float(y)+1.f,float(tz)+.5f},
                            radius,false,true};
                        plan.valid=true;break;
                    }
                }
            }
            found=cache.emplace(key,plan).first;
        }
        if(!found->second.valid) continue;
        const auto& cave=found->second.cave;
        if(std::max(cave.mouth.x,cave.target.x)+8<ox-6 || std::min(cave.mouth.x,cave.target.x)-8>ox+22 ||
           std::max(cave.mouth.z,cave.target.z)+8<oz-6 || std::min(cave.mouth.z,cave.target.z)-8>oz+22) continue;
        result.push_back(cave);
    }
    return result;
}
}
