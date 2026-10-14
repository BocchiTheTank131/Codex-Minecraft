#include "RiverNetworkV22.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace RiverNetworkV22 {
namespace {
constexpr int Spacing=192;
struct Node { Point p; float c; int dx=0,dz=0; bool directed=false; int discharge=0; };
using Edge=Reach;
struct Lake {Point p; float radius,level;};
struct Tile { int x,z; std::vector<Edge> edges; std::vector<Lake> lakes; };
struct Cache { std::uint32_t seed=0; bool initialized=false; std::unordered_map<std::uint64_t,Node> nodes; std::vector<Tile> tiles; };
thread_local Cache cache;
int floorCell(int x) {return int(std::floor(double(x)/Spacing));}
std::uint64_t key(int x,int z) {return (std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(z);}
std::uint32_t hash(int x,int z,std::uint32_t seed) {
 auto h=seed^(std::uint32_t(x)*0x9e3779b9U)^(std::uint32_t(z)*0x85ebca6bU);
 h^=h>>16;h*=0x7feb352dU;h^=h>>15;h*=0x846ca68bU;return h^(h>>16);
}
float smooth(float a,float b,float v) {const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
Node node(int x,int z,const std::function<Ground(int,int)>& ground) {
 auto found=cache.nodes.find(key(x,z));if(found!=cache.nodes.end())return found->second;
 const auto h=hash(x,z,cache.seed^0x18a1U);
 Point p{float(x*Spacing+Spacing/2)+float(int(h&255)-128)*.60f,
         float(z*Spacing+Spacing/2)+float(int((h>>8)&255)-128)*.60f};
 Node n{p,ground(int(p.x),int(p.z)).continent};
 cache.nodes.emplace(key(x,z),n);return n;
}
Node directed(int x,int z,const std::function<Ground(int,int)>& ground) {
 auto n=node(x,z,ground);if(n.directed)return n;
 float best=n.c;
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz) {
  const auto other=node(x+dx,z+dz,ground);
  // Strictly decreasing potential prevents cycles. Land channels terminate
  // at sea or a rare eligible lowland lake. No uphill links or drainage cycles.
  if(other.c<best){best=other.c;n.dx=dx;n.dz=dz;}
 }
 n.directed=true;cache.nodes[key(x,z)]=n;return n;
}
bool lakeEligible(int x,int z,const Node& n) {
 return !n.dx&&!n.dz && n.c>.025f && n.c<.15f && hash(x,z,cache.seed^0x22a4U)%5U==0;
}
bool reachesOcean(int x,int z,const std::function<Ground(int,int)>& ground) {
 for(int count=0;count<96;++count) {
  auto n=directed(x,z,ground);
  if(n.c<-.025f)return true;
  if(!n.dx&&!n.dz)return lakeEligible(x,z,n);
  x+=n.dx;z+=n.dz;
 }
 // The fixed path bound is evaluated from each origin; no truncated result
 // is memoized for other nodes. Cache warmth cannot change a drainage decision.

 return false;
}
int incomingDrainage(int x,int z,const std::function<Ground(int,int)>& ground) {
 int total=0;
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz) {
  const auto n=directed(x+dx,z+dz,ground);
  if(n.dx==-dx&&n.dz==-dz)++total;
 }
 return total;
}
int incomingStreams(int x,int z,const std::function<Ground(int,int)>& ground) {
 int total=0;
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz) {
  const auto n=directed(x+dx,z+dz,ground);
  if(n.dx==-dx&&n.dz==-dz &&
     (incomingDrainage(x+dx,z+dz,ground)||hash(x+dx,z+dz,cache.seed^0x18b2U)%3==0))++total;
 }
 return total;
}
int discharge(int x,int z,const std::function<Ground(int,int)>& ground) {
 const auto n=directed(x,z,ground);if(n.discharge)return n.discharge;
 // Exact catchment count up to the width saturation point. Every node has
 // one outlet, so its upstream tree has neither duplicates nor cycles.
 // No depth-truncated recursive result is cached or reused.
 std::array<std::pair<int,int>,32> queue{};queue[0]={x,z};int count=1;
 for(int i=0;i<count && count<32;++i) {
  const auto p=queue[i];
  for(int dz=-1;dz<=1 && count<32;++dz)for(int dx=-1;dx<=1 && count<32;++dx)if(dx||dz) {
   const auto other=directed(p.first+dx,p.second+dz,ground);
   if(other.dx==-dx&&other.dz==-dz)queue[count++]={p.first+dx,p.second+dz};
  }
 }
 cache.nodes[key(x,z)].discharge=count;return count;
}
Tile build(int cx,int cz,const std::function<Ground(int,int)>& ground) {
 Tile tile{cx,cz,{}, {}};tile.edges.reserve(25);
 // Include neighboring origins even when a curved reach doubles back across
 // a cell border. This envelope covers jitter and Hermite control excursions.
 for(int z=cz-2;z<=cz+2;++z)for(int x=cx-2;x<=cx+2;++x) {
  const auto n=directed(x,z,ground);
  if(lakeEligible(x,z,n) && incomingStreams(x,z,ground)>0)
      tile.lakes.push_back({n.p,22.f+float(hash(x,z,cache.seed^0x22b5U)%18U),64.f+std::clamp(n.c*55.f,0.f,36.f)});
  if(n.c<-.025f||(!n.dx&&!n.dz)||!reachesOcean(x,z,ground))continue;
  const auto end=node(x+n.dx,z+n.dz,ground);
  const int drainage=incomingDrainage(x,z,ground);
  if(!drainage&&hash(x,z,cache.seed^0x18b2U)%3)continue;
  // Count actual tributaries, not dry catchment neighbors. Spring-fed starts
  // without an upstream channel retain the tapered headwater profile.
  const int count=incomingStreams(x,z,ground);
  Edge e{};e.incoming=count;
  e.from=64.f+std::clamp(n.c*55.f,0.f,36.f);e.to=64.f+std::clamp(end.c*55.f,0.f,36.f);
  const int catchment=discharge(x,z,ground);
  e.halfWidth=catchment<5?1.3f+(catchment-1)*.55f:std::min(12.f,4.f+(catchment-5)*.30f);
  const float dx=end.p.x-n.p.x,dz=end.p.z-n.p.z,len=std::sqrt(dx*dx+dz*dz);
  const float bend=(float(hash(x,z,cache.seed^0x18c3U)&255)/255.f-.5f)*60.f;
  const auto next=directed(x+n.dx,z+n.dz,ground);
  const auto after=node(x+n.dx+next.dx,z+n.dz+next.dz,ground);
  const float nx=next.dx||next.dz?after.p.x-end.p.x:dx;
  const float nz=next.dx||next.dz?after.p.z-end.p.z:dz;
  const float nl=std::hypot(nx,nz);
  for(int i=0;i<9;++i) {
   const float t=float(i)/8.f,t2=t*t,t3=t2*t;
   const float wave=std::sin(t*3.141592654f),offset=wave*wave*bend+std::sin(t*6.283185308f)*wave*wave*bend*.35f;
   // Incoming reaches share the outgoing tangent at a junction, avoiding
   // sharp lattice-direction corners. Endpoints remain exactly shared.
   e.p[i]={(2*t3-3*t2+1)*n.p.x+(t3-2*t2+t)*dx+(-2*t3+3*t2)*end.p.x+(t3-t2)*nx/nl*len-dz/len*offset,
           (2*t3-3*t2+1)*n.p.z+(t3-2*t2+t)*dz+(-2*t3+3*t2)*end.p.z+(t3-t2)*nz/nl*len+dx/len*offset};
  }
  e.minX=e.maxX=e.p[0].x;e.minZ=e.maxZ=e.p[0].z;
  for(const auto p:e.p){e.minX=std::min(e.minX,p.x);e.maxX=std::max(e.maxX,p.x);e.minZ=std::min(e.minZ,p.z);e.maxZ=std::max(e.maxZ,p.z);}
  tile.edges.push_back(e);
 }
 return tile;
}
}
Sample sample(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground,float terrainHeight) {
 if(!cache.initialized||cache.seed!=seed){cache.nodes.clear();cache.tiles.clear();cache.nodes.reserve(8192);cache.tiles.reserve(16);cache.seed=seed;cache.initialized=true;}
 const int cx=floorCell(x),cz=floorCell(z);
 auto found=std::find_if(cache.tiles.begin(),cache.tiles.end(),[&](const Tile& t){return t.x==cx&&t.z==cz;});
 if(found==cache.tiles.end()) {
  if(cache.nodes.size()>8192)cache.nodes.clear();
  if(cache.tiles.size()==16)cache.tiles.erase(cache.tiles.begin());
  cache.tiles.push_back(build(cx,cz,ground));found=cache.tiles.end()-1;
 }
 Sample result;
 float best=1e9f;
 for(const auto& lake:found->lakes) {
  const float d=std::hypot(float(x)-lake.p.x,float(z)-lake.p.z);
  const float channel=1-smooth(lake.radius-2.f,lake.radius+2.f,d);
  if(channel>result.channel){result.channel=channel;result.waterLevel=lake.level;best=d-lake.radius;}
  result.valley=std::max(result.valley,1-smooth(lake.radius,lake.radius+14.f,d));
 }
 for(const auto& edge:found->edges) {
  const float radius=edge.halfWidth+48.f;
  if(x<edge.minX-radius||x>edge.maxX+radius||z<edge.minZ-radius||z>edge.maxZ+radius)continue;
  for(int i=0;i<8;++i) {
   const auto a=edge.p[i],b=edge.p[i+1];const float dx=b.x-a.x,dz=b.z-a.z;
   const float u=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/(dx*dx+dz*dz),0.f,1.f);
   const float rx=x-a.x-dx*u,rz=z-a.z-dz*u,d=std::sqrt(rx*rx+rz*rz);
   const float t=(i+u)/8.f;
   const float width=edge.halfWidth*(edge.incoming?1.f:.4f+.6f*smooth(0,.15f,t));
   const float level=edge.from+(edge.to-edge.from)*t;
   // Higher surrounding terrain needs a wider, dry valley shoulder. Water
   // stays confined to the channel; lowland banks retain the narrow profile.
   const float shoulder=std::clamp((terrainHeight-level)*1.25f,14.f,48.f);
   result.valley=std::max(result.valley,1-smooth(width,width+shoulder,d));
   const float score=d-width;
   if(score<best){best=score;result.channel=1-smooth(width-.5f,width+1.f,d);result.waterLevel=edge.from+(edge.to-edge.from)*t;}
  }
 }
 return result;
}
std::vector<Reach> inspectCell(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground) {
 RiverNetworkV22::sample(x,z,seed,ground);
 const int cx=floorCell(x),cz=floorCell(z);
 const auto found=std::find_if(cache.tiles.begin(),cache.tiles.end(),[&](const Tile& t){return t.x==cx&&t.z==cz;});
 std::vector<Reach> result;
 for(const auto& e:found->edges)if(floorCell(int(e.p[0].x))==cx&&floorCell(int(e.p[0].z))==cz)result.push_back(e);
 return result;
}
}
