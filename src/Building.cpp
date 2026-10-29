#include "Building.h"
#include "World.h"
#include "Automation.h"
#include <algorithm>
#include <cmath>
const std::array<BuildingDefinition,142>& buildingContent() {
 static const std::array<BuildingDefinition,142> content={{
#define BUILDING(name,shape,material,ingredient) {Block::name,Item::name,BuildingShape::shape,material,ingredient,#name},
#include "BuildingContent.inc"
#undef BUILDING
 }};return content;
}
const std::array<BuildingDefinition,15>& automationContent(){
 static const std::array<BuildingDefinition,15> content={{
#define AUTOMATION(name,shape,material,ingredient,label) {Block::name,Item::name,BuildingShape::shape,Block::material,Item::ingredient,label},
#include "AutomationContent.inc"
#undef AUTOMATION
 {Block::PistonHead,Item::None,BuildingShape::PistonHead,Block::Planks,Item::None,"Piston Head"}
 }};return content;
}
const BuildingDefinition* buildingDefinition(Block b){const auto t=blockType(b);if(automationBlock(b))return &automationContent()[int(t)-int(Block::PowerWire)];return t>=Block::OakStairs&&t<=Block::Scaffold?&buildingContent()[int(t)-int(Block::OakStairs)]:nullptr;}
const BuildingDefinition* buildingDefinition(Item i){if(i>=Item::PowerWire&&i<=Item::AutoHarvester)return &automationContent()[int(i)-int(Item::PowerWire)];return i>=Item::OakStairs && i<=Item::Scaffold?&buildingContent()[int(i)-int(Item::OakStairs)]:nullptr;}
glm::ivec3 buildingFacing(int f){constexpr glm::ivec3 d[]={{0,0,-1},{1,0,0},{0,0,1},{-1,0,0}};return d[f&3];}
bool buildingIsContainer(Block b){return blockType(b)==Block::Barrel||blockType(b)==Block::Hopper;}
bool buildingIsBed(Block b){return blockType(b)==Block::Bed;}
bool buildingIsDoor(Block b){auto d=buildingDefinition(b);return d && d->shape==BuildingShape::Door;}
bool buildingTransparent(Block b){auto d=buildingDefinition(b);return d && d->shape==BuildingShape::Pane;}
bool buildingOccludesLight(Block b){auto d=buildingDefinition(b);return d && (d->shape==BuildingShape::Cube || d->shape==BuildingShape::Machine || (d->shape==BuildingShape::Piston&&!(blockProperties(b)&8)) || d->shape==BuildingShape::Barrel || d->shape==BuildingShape::StrippedLog || (d->shape==BuildingShape::Slab && (blockProperties(b)&8)));}
bool buildingSolid(Block b){auto d=buildingDefinition(b);if(!d)return true;switch(d->shape){
 case BuildingShape::Wire:case BuildingShape::Lever:case BuildingShape::Circuit:case BuildingShape::Button:case BuildingShape::PressurePlate:case BuildingShape::Chain:case BuildingShape::Candle:case BuildingShape::Sign:case BuildingShape::Painting:case BuildingShape::Frame:return false;
 case BuildingShape::FenceGate:return !(blockProperties(b)&8);
 default:return true;}}
bool validBuildingState(Block b){if(automationBlock(b))return validAutomationState(b);auto d=buildingDefinition(b);if(!d)return false;unsigned mask=0;
 switch(d->shape){
 case BuildingShape::Cube:case BuildingShape::Path:case BuildingShape::Table:case BuildingShape::Pot:case BuildingShape::Scaffold:mask=0;break;
 case BuildingShape::Stairs:mask=119;break;
 case BuildingShape::Slab:mask=12;break;
 case BuildingShape::Fence:case BuildingShape::Wall:case BuildingShape::Pane:mask=240;break;
 case BuildingShape::Door:mask=155;break;
 case BuildingShape::Trapdoor:mask=15;break;
 case BuildingShape::FenceGate:mask=11;break;
 case BuildingShape::Lantern:mask=4;break;case BuildingShape::Button:mask=11;break;case BuildingShape::PressurePlate:case BuildingShape::Campfire:mask=8;break;case BuildingShape::Chain:mask=0;break;
 case BuildingShape::Carpet:mask=768;break;case BuildingShape::Candle:mask=776;break;
 case BuildingShape::Bed:mask=787;break;
 case BuildingShape::Sign:mask=19;break;case BuildingShape::Painting:mask=819;break;case BuildingShape::Frame:mask=771;break;
 case BuildingShape::StrippedLog:mask=3;break;
 default:mask=3;break;}
 const auto p=blockProperties(b);if(p&~mask)return false;
 if(d->shape==BuildingShape::Painting && (p&16) && !(p&32))return false;
 if(d->shape==BuildingShape::Slab && p==12)return false;
 if(d->shape==BuildingShape::Stairs && ((p>>4)&7)>4)return false;
 if(d->shape==BuildingShape::StrippedLog && p>2)return false;
 return true;}
std::uint8_t buildingEmission(Block b){const auto t=blockType(b);return t==Block::PowerLamp?((blockProperties(b)&0x0f00)?15:0):t==Block::Lantern?14:t==Block::Candle && !(blockProperties(b)&8)?10:t==Block::Campfire && !(blockProperties(b)&8)?15:0;}
bool buildingNeedsSupport(Block b){if(automationBlock(b)){auto t=blockType(b);return t==Block::PowerWire||t==Block::Lever||t==Block::Repeater||t==Block::AndGate||t==Block::OrGate||t==Block::NotGate||t==Block::DaylightSensor||t==Block::CropSensor;}auto d=buildingDefinition(b);if(!d)return false;switch(d->shape){case BuildingShape::Door:case BuildingShape::Bed:case BuildingShape::Chain:case BuildingShape::Lantern:case BuildingShape::Candle:case BuildingShape::Sign:case BuildingShape::Painting:case BuildingShape::Frame:case BuildingShape::Pot:case BuildingShape::Carpet:case BuildingShape::Button:case BuildingShape::PressurePlate:return true;default:return false;}}
BuildingBoxes buildingBoxes(Block b,bool collision){BuildingBoxes out;auto d=buildingDefinition(b);if(!d)return out;auto p=blockProperties(b);const int f=p&3;bool rotate=false;
 switch(d->shape){
 case BuildingShape::Wire:{out.add(.375f,0,.375f,.625f,.0625f,.625f);for(int dir=0;dir<4;++dir)if(p&(16<<dir)){auto v=buildingFacing(dir);out.add(v.x<0?0:.4375f,0,v.z<0?0:.4375f,v.x>0?1:.5625f,.0625f,v.z>0?1:.5625f);if(p&(4096<<dir))out.add(v.x<0?0:v.x>0?.9375f:.4375f,.0625f,v.z<0?0:v.z>0?.9375f:.4375f,v.x>0?1:v.x<0?.0625f:.5625f,1,v.z>0?1:v.z<0?.0625f:.5625f);}break;}
 case BuildingShape::Lever:out.add(.25f,0,.25f,.75f,.125f,.75f);out.add(.4375f,.125f,p&8?.5f:.25f,.5625f,.625f,p&8?.75f:.5f);rotate=true;break;
 case BuildingShape::Circuit:out.add(.0625f,0,.0625f,.9375f,.125f,.9375f);out.add(.4375f,.125f,.0625f,.5625f,.3125f,.25f);out.add(.25f,.125f,.625f,.375f,.25f,.8125f);out.add(.625f,.125f,.625f,.75f,.25f,.8125f);rotate=true;break;
 case BuildingShape::Hopper:out.add(0,.625f,0,.1875f,1,1);out.add(.8125f,.625f,0,1,1,1);out.add(.1875f,.625f,0,.8125f,1,.1875f);out.add(.1875f,.625f,.8125f,.8125f,1,1);out.add(.25f,.25f,.25f,.75f,.625f,.75f);if(p&4)out.add(.375f,0,.375f,.625f,.25f,.625f);else out.add(.375f,.25f,0,.625f,.5f,.5f);rotate=true;break;
 case BuildingShape::Piston:out.add(0,0,0,1,1,p&8?.75f:1);break;
 case BuildingShape::PistonHead:out.add(0,0,0,1,1,.25f);out.add(.375f,.375f,.25f,.625f,.625f,1);break;
 case BuildingShape::Stairs:{const bool top=p&4;const float y=top?.5f:0.f,sy=top?0.f:.5f;out.add(0,y,0,1,y+.5f,1);int shape=(p>>4)&7;
  if(shape<3)out.add(0,sy,.5f,1,sy+.5f,1);
  if(shape==1)out.add(0,sy,0,.5f,sy+.5f,.5f);
  if(shape==2)out.add(.5f,sy,0,1,sy+.5f,.5f);
  if(shape==3)out.add(0,sy,.5f,.5f,sy+.5f,1);
  if(shape==4)out.add(.5f,sy,.5f,1,sy+.5f,1);rotate=true;break;}
 case BuildingShape::Slab:out.add(0,p&4?.5f:0,0,1,p&8?1:p&4?1:.5f,1);break;
 case BuildingShape::Fence:case BuildingShape::Wall:{const bool wall=d->shape==BuildingShape::Wall;float lo=wall?.25f:.375f,hi=1-lo;out.add(lo,0,lo,hi,collision?1.5f:1,hi);
  for(int dir=0;dir<4;++dir)if(p&(16<<dir)){const auto v=buildingFacing(dir);float x=v.x<0?0:v.x>0?hi:lo,X=v.x>0?1:v.x<0?lo:hi,z=v.z<0?0:v.z>0?hi:lo,Z=v.z>0?1:v.z<0?lo:hi;
   if(collision)out.add(x,0,z,X,1.5f,Z);else if(wall)out.add(x,0,z,X,.875f,Z);else{out.add(x,.375f,z,X,.5f,Z);out.add(x,.75f,z,X,.875f,Z);}}break;}
 case BuildingShape::Pane:{out.add(.4375f,0,.4375f,.5625f,1,.5625f);for(int dir=0;dir<4;++dir)if(p&(16<<dir)){auto v=buildingFacing(dir);out.add(v.x<0?0:v.x>0?.5625f:.4375f,0,v.z<0?0:v.z>0?.5625f:.4375f,v.x>0?1:v.x<0?.4375f:.5625f,1,v.z>0?1:v.z<0?.4375f:.5625f);}break;}
 case BuildingShape::FenceGate:{if(p&8){if(!collision){out.add(0,0,.375f,.125f,1,.625f);out.add(.875f,0,.375f,1,1,.625f);}}else if(collision){out.add(0,.25f,.4375f,1,1.5f,.5625f);}else{
  // Visible gate frame and rails share the placed model and thumbnail. The
  // closed collision barrier stays identical to v2.16.0.
  out.add(0,.25f,.4375f,.125f,1,.5625f);out.add(.875f,.25f,.4375f,1,1,.5625f);
  out.add(.125f,.375f,.4375f,.875f,.5f,.5625f);out.add(.125f,.75f,.4375f,.875f,.875f,.5625f);
  out.add(.4375f,.375f,.4375f,.5625f,.875f,.5625f);
 }rotate=true;break;}
 case BuildingShape::Trapdoor:if(p&8){out.add(0,0,0,1,1,.1875f);}else out.add(0,p&4?.8125f:0,0,1,p&4?1:.1875f,1);rotate=true;break;
 case BuildingShape::Door:out.add(0,0,0,1,1,.1875f);rotate=true;break;
 case BuildingShape::Button:out.add(.3125f,.375f,0,.6875f,.625f,p&8?.0625f:.125f);rotate=true;break;
 case BuildingShape::PressurePlate:out.add(.0625f,0,.0625f,.9375f,p&8?.03125f:.0625f,.9375f);break;
 case BuildingShape::Table:out.add(0,.75f,0,1,1,1);for(float x:{.0625f,.8125f})for(float z:{.0625f,.8125f})out.add(x,0,z,x+.125f,.75f,z+.125f);break;
 case BuildingShape::Chair:case BuildingShape::Bench:{out.add(.0625f,.4375f,.0625f,.9375f,.5625f,.9375f);for(float x:{.0625f,.8125f})for(float z:{.0625f,.8125f})out.add(x,0,z,x+.125f,.4375f,z+.125f);if(d->shape==BuildingShape::Chair)out.add(.0625f,.5625f,.8125f,.9375f,1.f,.9375f);rotate=true;break;}
 case BuildingShape::Shelf:out.add(0,.4375f,.5f,1,.5625f,1);out.add(.0625f,0,.8125f,.1875f,.4375f,1);out.add(.8125f,0,.8125f,.9375f,.4375f,1);rotate=true;break;
 case BuildingShape::Sign:out.add(0,.375f,p&16?.875f:.4375f,1,.9375f,p&16?1:.5625f);if(!(p&16))out.add(.4375f,0,.4375f,.5625f,.375f,.5625f);rotate=true;break;
 case BuildingShape::Painting:out.add((p&32)&&(p&16)?0:.0625f,.0625f,.9375f,(p&32)&&!(p&16)?1:.9375f,.9375f,1);rotate=true;break;
 case BuildingShape::Frame:out.add(.0625f,.0625f,.9375f,.9375f,.9375f,1);rotate=true;break;
 case BuildingShape::Lantern:out.add(.3125f,p&4?.125f:0,.3125f,.6875f,p&4?.625f:.5f,.6875f);out.add(.4375f,p&4?.625f:.5f,.4375f,.5625f,p&4?1:.6875f,.5625f);break;
 case BuildingShape::Chain:out.add(.4375f,0,.4375f,.5625f,1,.5625f);break;
 case BuildingShape::Candle:out.add(.4375f,0,.4375f,.5625f,.4375f,.5625f);break;
 case BuildingShape::Campfire:out.add(.0625f,0,.0625f,.9375f,.25f,.9375f);out.add(.125f,.25f,.3125f,.875f,.4375f,.6875f);break;
 case BuildingShape::Bed:out.add(0,.1875f,0,1,.5625f,1);for(float x:{.0625f,.8125f})for(float z:{.0625f,.8125f})out.add(x,0,z,x+.125f,.1875f,z+.125f);rotate=true;break;
 case BuildingShape::Pot:out.add(.3125f,0,.3125f,.6875f,.375f,.6875f);out.add(.25f,.375f,.25f,.75f,.5f,.75f);break;
 case BuildingShape::Carpet:out.add(0,0,0,1,.0625f,1);break;
 case BuildingShape::Path:out.add(0,0,0,1,.9375f,1);break;
 case BuildingShape::Scaffold:out.add(0,.875f,0,1,1,1);for(float x:{0.f,.875f})for(float z:{0.f,.875f})out.add(x,0,z,x+.125f,.875f,z+.125f);break;
 default:out.add(0,0,0,1,1,1);break;}
 if(d->shape==BuildingShape::Piston||d->shape==BuildingShape::PistonHead){
  // Canonical front -Z: base keeps its rear when extended, head occupies the next cell.
  if(d->shape==BuildingShape::Piston && (p&8))for(int i=0;i<out.count;++i){out.boxes[i].minimum.z=.25f;out.boxes[i].maximum.z=1;}
  if(d->shape==BuildingShape::Piston && (p&8))out.add(.375f,.375f,0,.625f,.625f,.25f);
  if(p&4){for(int i=0;i<out.count;++i){auto& box=out.boxes[i];auto a=box.minimum,c=box.maximum;if(p&1){box.minimum={a.x,a.z,a.y};box.maximum={c.x,c.z,c.y};}else{box.minimum={a.x,1-c.z,a.y};box.maximum={c.x,1-a.z,c.y};}}}
  else rotate=true;
 }
 int rotation=f;if(d->shape==BuildingShape::Door && (p&8))rotation=(f+((p&128)?3:1))&3;
 if(rotate)for(int i=0;i<out.count;++i)for(int k=0;k<rotation;++k){auto& box=out.boxes[i];const float x=box.minimum.x,X=box.maximum.x;box.minimum.x=1-box.maximum.z;box.maximum.x=1-box.minimum.z;box.minimum.z=x;box.maximum.z=X;}
 return out;}
BlockGeometryProperties buildingGeometry(Block b){BlockGeometryProperties g;auto d=buildingDefinition(b);if(!d)return g;auto boxes=buildingBoxes(b);g.shape=buildingOccludesLight(b)?BlockShape::Cube:BlockShape::PartialCube;
 g.minX=g.minY=g.minZ=1;g.maxX=g.maxY=g.maxZ=0;for(int i=0;i<boxes.count;++i){const auto& box=boxes.boxes[i];g.minX=std::min(g.minX,box.minimum.x);g.minY=std::min(g.minY,box.minimum.y);g.minZ=std::min(g.minZ,box.minimum.z);g.maxX=std::max(g.maxX,box.maximum.x);g.maxY=std::max(g.maxY,box.maximum.y);g.maxZ=std::max(g.maxZ,box.maximum.z);}
 g.occludesNeighborFaces=g.shape==BlockShape::Cube;g.aoOcclusion=g.occludesNeighborFaces?1:.15f;g.mergeMatchingFaces=d->shape==BuildingShape::Pane;if(buildingTransparent(b)){g.occludesNeighborFaces=false;g.aoOcclusion=0;}return g;}
Block buildingConnectedState(const World& w,const glm::ivec3& pos,Block b){auto d=buildingDefinition(b);if(!d)return b;auto p=blockProperties(b);auto get=[&](glm::ivec3 v){return w.getBlock(v.x,v.y,v.z);};
 if(blockType(b)==Block::PowerWire){p&=0x0f00;for(int dir=0;dir<4;++dir){auto q=pos+buildingFacing(dir);auto n=get(q);bool up=false,connect=automationDevice(n);if(!connect&&isSolid(n)&&get(pos+glm::ivec3(0,1,0))==Block::Air){connect=blockType(get(q+glm::ivec3(0,1,0)))==Block::PowerWire;up=connect;}if(!connect&&!isSolid(n))connect=blockType(get(q-glm::ivec3(0,1,0)))==Block::PowerWire;if(connect){p|=16<<dir;if(up)p|=4096<<dir;}}return blockState(b,p);}

 if(d->shape==BuildingShape::Fence||d->shape==BuildingShape::Wall||d->shape==BuildingShape::Pane){p&=~240;for(int dir=0;dir<4;++dir){Block n=get(pos+buildingFacing(dir));auto nd=buildingDefinition(n);bool compatible=nd && (nd->shape==d->shape || (d->shape!=BuildingShape::Pane && nd->shape==BuildingShape::FenceGate && ((dir&1)!=(blockProperties(n)&1))));if(compatible || (d->shape==BuildingShape::Pane && n==Block::Glass) || blockGeometry(n).occludesNeighborFaces)p|=16<<dir;}}
 if(d->shape==BuildingShape::Stairs){p&=~112;int f=p&3;auto front=get(pos+buildingFacing(f)),back=get(pos-buildingFacing(f));auto matches=[&](Block n){auto nd=buildingDefinition(n);return nd && nd->shape==BuildingShape::Stairs && (blockProperties(n)&4)==(p&4);};
  if(matches(front) && ((blockProperties(front)&3)&1)!=(f&1)){int turn=((blockProperties(front)&3)-f+4)%4;p|=(turn==3?3:4)<<4;}
  else if(matches(back) && ((blockProperties(back)&3)&1)!=(f&1)){int turn=((blockProperties(back)&3)-f+4)%4;p|=(turn==3?1:2)<<4;}}
 return blockState(blockType(b),p);}
Block buildingPlacement(Block b,const glm::vec3& look,const glm::ivec3& normal,const glm::vec3& hit){auto d=buildingDefinition(b);if(!d)return b;int f=std::abs(look.x)>std::abs(look.z)?(look.x>0?1:3):(look.z>0?2:0);unsigned p=0;
 if(automationBlock(b)){p=(blockType(b)==Block::PowerWire||blockType(b)==Block::PowerLamp||blockType(b)==Block::DaylightSensor)?0:f;if(blockType(b)==Block::Hopper){p=normal.y?4:((normal.x>0?3:normal.x<0?1:normal.z>0?0:2));}else if(d->shape==BuildingShape::Piston){if(std::abs(look.y)>.7f)p=4|(look.y>0?1:0);else p=(f+2)&3;}return blockState(b,p);}
 switch(d->shape){case BuildingShape::Stairs:case BuildingShape::Trapdoor:p=d->shape==BuildingShape::Stairs?(f+2)&3:f;if(normal.y<0||(normal.y==0 && hit.y-std::floor(hit.y)>.5f))p|=4;break;
 case BuildingShape::Slab:if(normal.y<0||(normal.y==0 && hit.y-std::floor(hit.y)>.5f))p=4;break;
 case BuildingShape::Lantern:p=normal.y<0?4:0;break;
 case BuildingShape::StrippedLog:p=normal.x?1:normal.z?2:0;break;
 case BuildingShape::Painting:case BuildingShape::Frame:p=normal.x>0?1:normal.x<0?3:normal.z>0?2:0;break;
 case BuildingShape::Sign:p=(f+2)&3;if(normal.y==0)p=(normal.x>0?1:normal.x<0?3:normal.z>0?2:0)|16;break;
 case BuildingShape::Door:case BuildingShape::FenceGate:case BuildingShape::Chair:case BuildingShape::Bench:case BuildingShape::Shelf:case BuildingShape::Bed:case BuildingShape::Button:case BuildingShape::Barrel:p=f;break;
 default:break;}
 return blockState(b,std::uint16_t(p));}
int buildingTexture(Block b,int face){const auto d=buildingDefinition(b);if(!d)return 3;const auto t=blockType(b);unsigned p=blockProperties(b);
 if(automationBlock(b)){int index=int(t)-int(Block::PowerWire);
  if(d->shape==BuildingShape::Machine||d->shape==BuildingShape::Piston){constexpr glm::ivec3 normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};if(normals[face]!=automationFacing(b))return blockTexture(d->material,face);}
  bool on=automationSignal(b)>0||(p&8)||(t==Block::PowerLamp&&(p&0x0f00));return 187+index*2+(on?1:0);}
 if(d->shape==BuildingShape::StrippedLog){const int axis=p&3;const bool end=axis==0?(face==2||face==3):axis==1?(face==0||face==1):(face==4||face==5);return end?blockDefinition(d->material).topTexture:143+(int(t)-int(Block::OakStrippedLog))/14;}
 if(t>=Block::ChiseledStoneBricks && t<=Block::RoofShingles)return 149+int(t)-int(Block::ChiseledStoneBricks);
 if(t==Block::StonePaving)return 157;
 if(d->shape==BuildingShape::Pane)return 158+int(t)-int(Block::GlassPane);
 if(t==Block::Lantern)return 162;if(t==Block::Chain)return 163;if(t==Block::Candle)return 164+((p>>8)&3);
 if(t==Block::Campfire)return p&8?169:168;if(t==Block::Bed)return face==2?((p&16)?170:181)+((p>>8)&3):blockTexture(Block::Planks,face);
 if(t==Block::Barrel)return 174;if(t==Block::Painting)return 175+((p>>8)&3);if(t==Block::ItemFrame)return 179;if(t==Block::FlowerPot)return 180;if(t==Block::Carpet)return 181+((p>>8)&3);if(t==Block::DirtPath)return 185;if(t==Block::Scaffold)return 186;
 return blockTexture(d->material,face);
}

std::optional<glm::ivec3> blockSupportPosition(glm::ivec3 pos,Block b){
 const auto props=blockProperties(b);if(isTorch(b)&&props)return pos-buildingFacing(props-1);
 if(isVine(b)||isLadder(b)){constexpr glm::ivec3 wall[]={{0,0,-1},{0,0,1},{1,0,0},{-1,0,0}};return pos+wall[int(b)-int(isVine(b)?Block::VineNorth:Block::LadderNorth)];}
 if(auto d=buildingDefinition(b);d&&buildingNeedsSupport(b)){
  if(d->shape==BuildingShape::Chain||(d->shape==BuildingShape::Lantern&&(props&4)))return pos+glm::ivec3(0,1,0);
  if(d->shape==BuildingShape::Frame||d->shape==BuildingShape::Painting||d->shape==BuildingShape::Button||(d->shape==BuildingShape::Sign&&(props&16)))return pos+(d->shape==BuildingShape::Button?buildingFacing(props):-buildingFacing(props));
  return pos+glm::ivec3(0,-1,0);
 }
 if(isTorch(b)||isPlant(b)||isCrop(b)||isBerryBush(b)||b==Block::Cactus||b==Block::SugarCane||b==Block::Bamboo||b==Block::Fire||isDoor(b))return pos+glm::ivec3(0,-1,0);
 return {};
}
