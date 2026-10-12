#include "UndergroundStructures.h"
#include "TerrainDensity.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace {
int divFloor(int n,int d){int q=n/d;return q-(n%d<0);}
using Key=std::tuple<int,int,int>;
bool ocean(Biome b) {return b==Biome::Ocean||b==Biome::ColdOcean||b==Biome::WarmOcean||b==Biome::FrozenOcean;}
}
const char* UndergroundStructures::name(UndergroundKind k) {
    return k==UndergroundKind::Mineshaft?"Mineshaft":k==UndergroundKind::Dungeon?"Dungeon":"Ancient Ruins";
}
std::shared_ptr<const UndergroundPlan> UndergroundStructures::plan(int rx,int rz,const Site& site,const Cave& cave,const Reserved& reserved) const {
    const auto key=std::int64_t((std::uint64_t(std::uint32_t(rx))<<32U)|std::uint32_t(rz));
    {std::lock_guard<std::mutex> lock(mutex_);auto it=cache_.find(key);if(it!=cache_.end())return it->second;}
    auto result=build(rx,rz,site,cave,reserved);
    std::lock_guard<std::mutex> lock(mutex_);
    auto found=cache_.find(key);if(found!=cache_.end())return found->second;
    // Include rejected regions: ordinary chunks never repeat expensive cave searches.
    while(order_.size()>=128) {cache_.erase(order_.front());order_.pop_front();}
    cache_.emplace(key,result);order_.push_back(key);return result;
}
std::shared_ptr<const UndergroundPlan> UndergroundStructures::build(int rx,int rz,const Site& site,const Cave& cave,const Reserved& reserved) const {
    std::uint32_t rng=TerrainDensity::hash(rx,19,rz,seed_^0x4b120aU);
    const unsigned selection=rng%100U;
    if(selection>=34U)return {};
    const auto next=[&]() {rng=TerrainDensity::hash(int(rng),rx,rz,seed_^0xabe9U);return rng;};
    const int x=rx*RegionSize+128+int(next()%65U)-32,z=rz*RegionSize+128+int(next()%65U)-32;
    const auto local=site(x,z);
    if(ocean(local.biome)||local.biome==Biome::MushroomFields||local.height<68||local.river>.01f)return {};
    auto p=std::make_shared<UndergroundPlan>();p->regionX=rx;p->regionZ=rz;p->biome=local.biome;
    p->kind=selection<12?UndergroundKind::Mineshaft:selection<30?UndergroundKind::Dungeon:UndergroundKind::AncientRuins;
    int base=p->kind==UndergroundKind::Mineshaft?-20+int(next()%77U):
             p->kind==UndergroundKind::Dungeon?-44+int(next()%89U):-46+int(next()%65U);
    // Deep ruins have an additional rarity gate.
    if(p->kind==UndergroundKind::AncientRuins&&base<-24&&next()%2U)return {};
    base=std::min(base,local.height-22);
    // Every structure has a verified dry natural cave connection, rather than a
    // decorative sealed opening. Search a bounded number of world coordinates.
    bool found=false;int ex=0,ey=0,ez=0;
    constexpr int directions[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
    const int rotation=int(next()%4U);
    for(int i=0;i<4&&!found;++i)for(int distance=56;distance<=72&&!found;distance+=4)
        for(int dy=-4;dy<=4&&!found;dy+=2) {
            const auto& d=directions[(i+rotation)%4];
            const int tx=x+d[0]*distance,tz=z+d[1]*distance,ty=base+dy;
            if(ty<-50||site(tx,tz).height<ty+12)continue;
            if(cave(tx,ty+1,tz)) {ex=tx;ey=ty;ez=tz;found=true;}
        }
    if(!found)return {};
    p->entrance={ex,ey+1,ez};
    const int target=p->kind==UndergroundKind::Dungeon?1+(next()%5U==0?2:next()%3U==0?1:0):
                     p->kind==UndergroundKind::Mineshaft?4+int(next()%4U):3+int(next()%4U);
    std::set<std::pair<int,int>> occupied{{0,0}};
    std::vector<std::pair<int,int>> grid{{0,0}};std::vector<int> parent{-1};
    // A bounded randomized spanning tree guarantees connectivity, with optional
    // loops added later. Grid coordinates are layout scaffolding, not placement.
    for(int attempts=0;int(grid.size())<target&&attempts<80;++attempts) {
        int from=int(next()%unsigned(grid.size()));auto d=directions[next()%4U];
        const auto g=std::make_pair(grid[from].first+d[0],grid[from].second+d[1]);
        if(std::abs(g.first)>2||std::abs(g.second)>2||!occupied.insert(g).second)continue;
        grid.push_back(g);parent.push_back(from);
    }
    for(std::size_t i=0;i<grid.size();++i) {
        const int y=i==0?base:std::clamp(p->rooms[parent[i]].y+int(next()%5U)-2,-48,base+6);
        const int half=p->kind==UndergroundKind::Mineshaft?3+int(next()%2U):
            p->kind==UndergroundKind::Dungeon?3+int(next()%4U):4+int(next()%2U);
        p->rooms.push_back({x+grid[i].first*20,y,z+grid[i].second*20,half,3+int(next()%3U),
            p->kind==UndergroundKind::Mineshaft?4:4+int(next()%2U)});
    }
    // Region margin bounds neighboring underground plans by construction.
    // Reject all surface structure footprints, including their shallow foundations.
    p->bounds={x-76,base-6,z-76,x+76,base+14,z+76};
    if(reserved(p->bounds))return {};
    for(int dz=-48;dz<=48;dz+=16)for(int dx=-48;dx<=48;dx+=16)
        if(site(x+dx,z+dz).height<p->bounds.maxY+8)return {};
    std::map<Key,Block> cells;
    const auto put=[&](int wx,int wy,int wz,Block b) {cells[{wx,wy,wz}]=b;};
    const auto material=[&](int wx,int wy,int wz) {
        const auto h=TerrainDensity::hash(wx,wy,wz,seed_^0xbadcU)%12U;
        if(p->kind==UndergroundKind::Mineshaft)return Block::SprucePlanks;
        if(h<3)return p->kind==UndergroundKind::Dungeon?Block::MossyCobblestone:Block::MossyStoneBricks;
        if(h<5)return Block::CrackedStoneBricks;
        return p->kind==UndergroundKind::Dungeon?Block::Cobblestone:Block::StoneBricks;
    };
    for(const auto& r:p->rooms) {
        for(int dz=-r.halfZ;dz<=r.halfZ;++dz)for(int dx=-r.halfX;dx<=r.halfX;++dx)
            for(int dy=-1;dy<=r.height;++dy) {
                const bool wall=std::abs(dx)==r.halfX||std::abs(dz)==r.halfZ;
                Block b=dy<=0?material(r.x+dx,r.y+dy,r.z+dz):
                    dy==r.height||wall?material(r.x+dx,r.y+dy,r.z+dz):Block::Air;
                if(p->kind==UndergroundKind::Mineshaft&&(dy==r.height||wall))b=Block::Stone;
                put(r.x+dx,r.y+dy,r.z+dz,b);
            }
    }
    const auto corridor=[&](const UndergroundRoom& a,int bx,int by,int bz,bool mouth) {
        int px=a.x,pz=a.z;const int distance=std::abs(bx-px)+std::abs(bz-pz);
        std::vector<int> levels(std::size_t(distance)+1,-10000);
        for(int s=0;s<=distance;++s) {
            const int tx=a.x+(bx==a.x?0:(bx>a.x?1:-1)*s);
            const int tz=a.z+(bz==a.z?0:(bz>a.z?1:-1)*s);
            for(const auto& room:p->rooms)
                if(std::abs(tx-room.x)<=room.halfX+2&&std::abs(tz-room.z)<=room.halfZ+2) {levels[s]=room.y;break;}
        }
        levels.front()=a.y;levels.back()=by;
        for(int start=0;start<distance;) {
            int end=start+1;while(end<distance&&levels[end]==-10000)++end;
            for(int s=start+1;s<end;++s)levels[s]=levels[start]+(levels[end]-levels[start])*(s-start)/(end-start);
            start=end;
        }
        for(int s=0;s<=distance;++s) {
            // Full-block stairs, at most one block per step, with three blocks
            // of headroom. Portals overwrite room walls after shell placement.
            int y=levels[s];
            for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx) {
                const bool edge=bx==a.x?std::abs(dx)==2:std::abs(dz)==2;
                put(px+dx,y-1,pz+dz,material(px+dx,y-1,pz+dz));
                put(px+dx,y,pz+dz,material(px+dx,y,pz+dz));
                for(int h=1;h<=3;++h) {
                    // A later corridor's shell must not seal an existing junction.
                    auto key=Key{px+dx,y+h,pz+dz};auto it=cells.find(key);
                    if(!edge||it==cells.end()||it->second!=Block::Air)
                        put(px+dx,y+h,pz+dz,edge?Block::Stone:Block::Air);
                }
                auto roof=cells.find({px+dx,y+4,pz+dz});
                if(roof==cells.end()||roof->second!=Block::Air)put(px+dx,y+4,pz+dz,Block::Stone);
            }
            if(p->kind==UndergroundKind::Mineshaft&&s%6==3&&s>4&&s<distance-4) {
                const bool alongX=bx!=a.x;
                for(int h=1;h<=3;++h)for(int side:{-2,2})
                    put(px+(alongX?0:side),y+h,pz+(alongX?side:0),Block::SpruceLog);
                for(int side=-2;side<=2;++side)put(px+(alongX?0:side),y+3,pz+(alongX?side:0),Block::SprucePlanks);
                // Beam at headroom height; navigable space remains two blocks.
                put(px,y+1,pz+1,Block::Torch);
            }
            if(s<distance) {if(px!=bx)px+=bx>px?1:-1;else if(pz!=bz)pz+=bz>pz?1:-1;}
        }
        if(mouth) {
            // Open the shell into the verified dry cave. Retain a supported lip.
            for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)
                for(int h=1;h<=3;++h)put(bx+dx,by+h,bz+dz,Block::Air);
        }
    };
    for(std::size_t i=1;i<p->rooms.size();++i) {
        const auto& r=p->rooms[i];corridor(p->rooms[parent[i]],r.x,r.y,r.z,false);
    }
    if(p->rooms.size()>4&&next()%2U==0) {
        for(std::size_t i=1;i<grid.size();++i)for(std::size_t j=i+1;j<grid.size();++j)
            if(int(j)!=parent[i]&&int(i)!=parent[j]&&std::abs(grid[i].first-grid[j].first)+std::abs(grid[i].second-grid[j].second)==1)
                corridor(p->rooms[i],p->rooms[j].x,p->rooms[j].y,p->rooms[j].z,false);
    }
    corridor(p->rooms.front(),ex,ey,ez,true);
    if(p->kind==UndergroundKind::Mineshaft&&next()%2U==0) {
        const auto& r=p->rooms.back();const auto g=grid.back();
        for(int i=0;i<4;++i) {
            const auto& d=directions[(i+rotation)%4];
            if(occupied.count({g.first+d[0],g.second+d[1]}))continue;
            const int tx=r.x+d[0]*12,tz=r.z+d[1]*12;
            if((ex==tx&&std::abs(ez-tz)<5)||(ez==tz&&std::abs(ex-tx)<5))continue;
            const auto existing=cells.find({tx,r.y+1,tz});
            if(existing!=cells.end()&&existing->second==Block::Air)continue;
            corridor(r,tx,r.y,tz,false);
            // A short collapsed dead end branches off the navigable network.
            for(int side=-1;side<=1;++side)for(int h=1;h<=3;++h)
                put(tx+(d[0]?0:side),r.y+h,tz+(d[0]?side:0),h==1?Block::Gravel:Block::Cobblestone);
            break;
        }
    }
    // Interior rooms have flat platforms. Corridor joins cannot lift their
    // floors or leave stair treads in storage/encounter space.
    for(const auto& r:p->rooms)
        for(int dz=-r.halfZ+1;dz<r.halfZ;++dz)for(int dx=-r.halfX+1;dx<r.halfX;++dx) {
            put(r.x+dx,r.y,r.z+dz,material(r.x+dx,r.y,r.z+dz));
            for(int h=1;h<r.height;++h)put(r.x+dx,r.y+h,r.z+dz,Block::Air);
        }
    const auto table=p->kind==UndergroundKind::Mineshaft?StructureLoot::Mineshaft:
                     p->kind==UndergroundKind::Dungeon?StructureLoot::Dungeon:StructureLoot::Ancient;
    for(std::size_t i=0;i<p->rooms.size();++i) {
        const auto& r=p->rooms[i];
        // Furnishing stays out of connector axes. Torches have actual support.
        put(r.x-1,r.y+1,r.z-1,Block::Torch);
        if(i==0||i+1==p->rooms.size()||next()%3U==0) {
            put(r.x+r.halfX-1,r.y+1,r.z+r.halfZ-1,Block::Chest);
            p->loot.push_back({r.x+r.halfX-1,r.y+1,r.z+r.halfZ-1,table,next()});
        }
        if(p->kind==UndergroundKind::Mineshaft) {
            for(int side:{-1,1})for(int h=1;h<r.height;++h)
                put(r.x+side*(r.halfX-1),r.y+h,r.z-r.halfZ+1,Block::SpruceLog);
            put(r.x-r.halfX+1,r.y+2,r.z+r.halfZ-1,Block::Cobweb);
            put(r.x+r.halfX-1,r.y+3,r.z-r.halfZ+1,Block::Cobweb);
        } else if(p->kind==UndergroundKind::AncientRuins&&i+1==p->rooms.size()) {
            // A secluded library alcove screens the final treasure without a
            // sealed door or an inaccessible chest.
            put(r.x+r.halfX-2,r.y+1,r.z+r.halfZ-2,Block::Bookshelf);
            put(r.x+r.halfX-2,r.y+2,r.z+r.halfZ-2,Block::Bookshelf);
            put(r.x-r.halfX+1,r.y+3,r.z+r.halfZ-1,Block::Cobweb);
        }
        // Collapsed corners/alcoves never close the guaranteed central route.
        if(next()%3U==0) {
            put(r.x-r.halfX+1,r.y+1,r.z+r.halfZ-1,Block::Gravel);
            put(r.x-r.halfX+1,r.y+2,r.z+r.halfZ-1,Block::Cobblestone);
        }
        const int limit=p->kind==UndergroundKind::Dungeon?(base<0?3:1):p->kind==UndergroundKind::AncientRuins?2:1;
        if(int(p->mobs.size())<limit&&((p->kind==UndergroundKind::Dungeon)||i>0)&&
           std::abs(r.x-ex)+std::abs(r.z-ez)>=12) {
            p->mobs.push_back({r.x-1,r.y+1,r.z+1,true});
            if(p->kind==UndergroundKind::Dungeon&&base<0&&p->rooms.size()==1)
                p->mobs.push_back({r.x+1,r.y+1,r.z+1,true});
        }
    }
    p->bounds={x,base,z,x,base,z};p->cells.reserve(cells.size());
    for(const auto& cell:cells) {
        const auto [wx,wy,wz]=cell.first;
        p->cells.push_back({wx,wy,wz,cell.second,cell.second==Block::Air?TemplateOperation::Clear:TemplateOperation::Place});
        auto& b=p->bounds;b.minX=std::min(b.minX,wx);b.minY=std::min(b.minY,wy);b.minZ=std::min(b.minZ,wz);
        b.maxX=std::max(b.maxX,wx);b.maxY=std::max(b.maxY,wy);b.maxZ=std::max(b.maxZ,wz);
    }
    return p;
}
bool UndergroundStructures::apply(int cx,int cz,std::vector<Block>& blocks,std::vector<StructureLootMarker>& loot,
    std::vector<StructureMobMarker>& mobs,const Site& site,const Cave& cave,const Reserved& reserved,int minY,int maxY) const {
    // Plans fit entirely inside their 256-block region; one lookup per chunk.
    const auto p=plan(divFloor(cx*16,RegionSize),divFloor(cz*16,RegionSize),site,cave,reserved);
    if(!p||!p->bounds.intersectsXZ(cx*16,cz*16,cx*16+15,cz*16+15))return false;
    // Foundations reach natural ground when a room intersects a cavern. The
    // search is only four columns per room; platforms stay connected to supports.
    for(const auto& r:p->rooms)for(int dx:{-r.halfX,r.halfX})for(int dz:{-r.halfZ,r.halfZ}) {
        int x=r.x+dx-cx*16,z=r.z+dz-cz*16;if(x<0||x>=16||z<0||z>=16)continue;
        for(int y=r.y-2;y>=minY+4;--y) {
            auto index=std::size_t(((y-minY)*16+z)*16+x);
            if(index>=blocks.size())continue;
            if(isSolid(blocks[index]))break;
            blocks[index]=p->kind==UndergroundKind::Mineshaft?Block::SpruceLog:Block::StoneBricks;
        }
    }
    // Cells sorted by world X: skip whole columns outside this chunk.
    auto it=std::lower_bound(p->cells.begin(),p->cells.end(),cx*16,[](const StructureCell& c,int x){return c.x<x;});
    for(;it!=p->cells.end()&&it->x<cx*16+16;++it) {
        const auto& c=*it;if(c.z<cz*16||c.z>=cz*16+16||c.y<minY||c.y>=maxY)continue;
        const auto index=std::size_t(((c.y-minY)*16+c.z-cz*16)*16+c.x-cx*16);
        if(index>=blocks.size())blocks.resize(((index/4096)+1)*4096,Block::Air);
        blocks[index]=c.block;
    }
    const auto inside=[&](int x,int z){return x>=cx*16&&x<cx*16+16&&z>=cz*16&&z<cz*16+16;};
    for(const auto& m:p->loot)if(inside(m.x,m.z))loot.push_back(m);
    for(const auto& m:p->mobs)if(inside(m.x,m.z))mobs.push_back(m);
    return true;
}
