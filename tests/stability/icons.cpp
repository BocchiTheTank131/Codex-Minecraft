#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "Renderer.h"
#include "World.h"
#include "Player.h"
#include "Building.h"
#include "Definitions.h"
#include "Transportation.h"
#include "UiLayout.h"
#include "Screenshot.h"
#include "WorldTime.h"
#include <thread>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <iomanip>

namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point p){return std::chrono::duration<double,std::milli>(Clock::now()-p).count();}
struct PreviewVertex {glm::vec3 p,n;glm::vec2 uv;float tile;glm::vec3 tint{1};};
struct Preview {std::vector<PreviewVertex> vertices;Block block=Block::Air;std::string category;};
void raw(const std::filesystem::path& path,int w,int h,const std::vector<unsigned char>& p){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&w),4);f.write(reinterpret_cast<const char*>(&h),4);f.write(reinterpret_cast<const char*>(p.data()),p.size());if(!f)throw std::runtime_error("thumbnail write failed");}
void box(std::vector<PreviewVertex>& v,glm::vec3 lo,glm::vec3 hi,glm::vec2 uv,float tile=-2,glm::vec3 color={1,1,1}){
 const glm::vec3 normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
 for(int face=0;face<6;++face){auto n=normals[face];int axis=n.x?0:n.y?1:2,A=(axis+1)%3,B=(axis+2)%3;
  glm::vec3 p[4];for(int i=0;i<4;++i){p[i]=lo;p[i][axis]=n[axis]>0?hi[axis]:lo[axis];p[i][A]=(i==1||i==2)?hi[A]:lo[A];p[i][B]=(i>=2)?hi[B]:lo[B];}
  for(int i:{0,1,2,0,2,3})v.push_back({p[i],n,uv,tile,color});
 }
}
constexpr auto vs=R"(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec3 n;layout(location=2)in vec2 uv;layout(location=3)in float tile;layout(location=4)in vec3 tint;
uniform mat4 mvp;out vec3 N;out vec2 U;out float T;out vec3 C;
void main(){gl_Position=mvp*vec4(p,1);N=n;U=uv;T=tile;C=tint;})";
constexpr auto fs=R"(#version 330 core
in vec3 N;in vec2 U;in float T;in vec3 C;uniform sampler2D atlas;uniform int tiles;out vec4 color;
void main(){vec2 q=U;if(T>=0)q=vec2((T+clamp(fract(U.x),.03125,.96875))/float(tiles),clamp(fract(U.y),.03125,.96875));
 vec4 t=texture(atlas,q);if(t.a<.04)discard;vec3 n=normalize(N);float shade=.55+.4*max(n.y,0)+.22*max(-n.z,0)+.07*max(n.x,0);
 color=vec4(t.rgb*C*shade,t.a);})";
}
class IconTest {
 static Preview model(Item);
 static int generate(Game&,const std::filesystem::path&,const std::filesystem::path&);
 static int ui(Game&,const std::filesystem::path&,const std::filesystem::path&);
public:static int run(int,char**);
};
int runIconTests(int argc,char** argv){return IconTest::run(argc,argv);}
Preview IconTest::model(Item item){
 Preview p;p.block=itemToBlock(item);if(p.block==Block::Air)return p;
 p.category="world-mesh";World::MeshInput input;input.meshHeight=6;input.buildingShapes=true;
 input.blocks.assign(22*22*6,Block::Air);input.packedLight.assign(input.blocks.size(),0xf0);
 auto put=[&](int x,int y,int z,Block b){input.blocks[(y*22+z+3)*22+x+3]=b;};
 Block state=p.block;
 if(auto d=buildingDefinition(state)){
  if(d->shape==BuildingShape::Fence || d->shape==BuildingShape::Wall || d->shape==BuildingShape::Pane)state=blockState(state,32|128);
  if(d->shape==BuildingShape::Door)put(8,2,8,blockState(state,16));
  if(d->shape==BuildingShape::Bed)put(8,1,7,blockState(state,16));
 }
 if(state==Block::WoodenDoor){state=Block::DoorClosedNorthLower;put(8,2,8,Block::DoorClosedNorthUpper);}
 // Seeds are seed packets; edible roots/berries stay items rather than juvenile crops.
 if(isCrop(state) || isBerryBush(state)){p.block=Block::Air;return p;}
 put(8,1,8,state);auto mesh=World::buildMesh(input);
 mesh.opaque.insert(mesh.opaque.end(),mesh.water.begin(),mesh.water.end());
 for(const auto& v:mesh.opaque){const auto rgb=v.atlasTile.packed>>8;glm::vec3 tint((rgb&255)/255.f,((rgb>>8)&255)/255.f,((rgb>>16)&255)/255.f);
  p.vertices.push_back({v.position,v.normal,v.uv,float(v.atlasTile),tint});}
 p.block=state;return p;
}
int IconTest::generate(Game& g,const std::filesystem::path& output,const std::filesystem::path& source){
 std::filesystem::create_directories(output);auto started=Clock::now();
 std::ifstream f(source,std::ios::binary);int sw=0,sh=0;f.read(reinterpret_cast<char*>(&sw),4);f.read(reinterpret_cast<char*>(&sh),4);
 if(sw!=160||sh<16||sh>4096)throw std::runtime_error("Invalid source artwork atlas");std::vector<unsigned char> art(sw*sh*4);f.read(reinterpret_cast<char*>(art.data()),art.size());if(!f)throw std::runtime_error("Truncated artwork");
 GLuint sourceTex;glGenTextures(1,&sourceTex);glBindTexture(GL_TEXTURE_2D,sourceTex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,sw,sh,0,GL_RGBA,GL_UNSIGNED_BYTE,art.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 constexpr int S=128,cols=10,rows=(int(Item::Count)+9)/10;std::vector<unsigned char> atlas(cols*S*rows*S*4),pixels(S*S*4);
 GLuint fb,texture,depth,vao,vbo;glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,S,S,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,S,S);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Thumbnail FBO incomplete");
 const auto vertex=Renderer::compileShader(GL_VERTEX_SHADER,vs),fragment=Renderer::compileShader(GL_FRAGMENT_SHADER,fs),program=Renderer::linkProgram(vertex,fragment);glDeleteShader(vertex);glDeleteShader(fragment);
 glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
 for(int attr=0;attr<5;++attr){const int sizes[]={3,3,2,1,3};const std::size_t offsets[]={offsetof(PreviewVertex,p),offsetof(PreviewVertex,n),offsetof(PreviewVertex,uv),offsetof(PreviewVertex,tile),offsetof(PreviewVertex,tint)};glEnableVertexAttribArray(attr);glVertexAttribPointer(attr,sizes[attr],GL_FLOAT,GL_FALSE,sizeof(PreviewVertex),reinterpret_cast<void*>(offsets[attr]));}
 glViewport(0,0,S,S);glUseProgram(program);glUniform1i(glGetUniformLocation(program,"atlas"),0);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);
 // Straight-alpha result: blend RGB normally but preserve coverage in destination alpha.
 glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
 std::ofstream coverage(output/"coverage.csv");coverage<<"item,sprite,name,category,block,state,vertices,render_ms\n";
 for(int id=1;id<int(Item::Count);++id){const auto t=Clock::now();const auto item=Item(id);auto preview=model(item);GLuint material=g.renderer_->atlasTexture_;int tiles=BlockAtlasTiles;
  if(isBoatItem(item)){
   preview.category="boat-model";Transportation boats({0,0,0});boats.boats_.push_back({1,item});std::vector<RenderCuboid> parts;boats.appendModels(parts,{0,0,0},4);preview.vertices.clear();material=g.renderer_->boatTexture_;tiles=6;
   for(auto part:parts){auto base=preview.vertices.size();box(preview.vertices,part.center-part.size*.5f,part.center+part.size*.5f,{0,0},float(part.woodTexture),part.color);
    for(std::size_t i=base;i<preview.vertices.size();++i){auto& v=preview.vertices[i];const int axis=std::abs(v.n.y)>.5?1:std::abs(v.n.x)>.5?0:2;v.uv=axis==1?glm::vec2(v.p.x,v.p.z):axis==0?glm::vec2(v.p.z,-v.p.y):glm::vec2(v.p.x,-v.p.y);}}
  }else if(preview.vertices.empty()){
   preview.category="extruded-item-art";preview.block=Block::Air;material=sourceTex;tiles=10;const int cell=itemDefinition(item).spriteIndex;const int ox=cell%10*16,oy=cell/10*16;
   for(int y=0;y<16;++y)for(int x=0;x<16;++x){const int index=((oy+y)*sw+ox+x)*4;if(art[index+3]<100)continue;
    const glm::vec2 uv((ox+x+.5f)/sw,(oy+y+.5f)/sh);box(preview.vertices,{x/16.f,(15-y)/16.f,0},{(x+1)/16.f,(16-y)/16.f,.075f},uv);}
  }
  if(preview.vertices.empty())throw std::runtime_error("Missing model: "+itemDefinition(item).displayName);
  glm::vec3 low(1e6),high(-1e6);for(auto v:preview.vertices){low=glm::min(low,v.p);high=glm::max(high,v.p);}const auto center=(low+high)*.5f;
  const bool flat=preview.category=="extruded-item-art";const auto direction=glm::normalize(flat?glm::vec3(.22f,.12f,1):glm::vec3(1,.81649658f,-1));
  auto view=glm::lookAt(center+direction*12.f,center,{0,1,0});glm::vec2 bounds(0);for(auto v:preview.vertices){auto q=view*glm::vec4(v.p,1);bounds=glm::max(bounds,glm::abs(glm::vec2(q)));}
  // Cubes share scale; shorter pieces retain useful width while thin objects get
  // bounded shape-specific framing. No silhouette is cropped or independently stretched.
  const float half=std::max(bounds.x,bounds.y)*1.13f;auto mvp=glm::ortho(-half,half,-half,half,.1f,30.f)*view;
  // Draw surfaces from back to front so glass borders remain visible through
  // nearer translucent faces. Depth testing still rejects coincident backfaces.
  if(!flat){std::vector<std::size_t> order(preview.vertices.size()/3);for(std::size_t j=0;j<order.size();++j)order[j]=j;
   std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){auto depthAt=[&](auto i){return glm::dot((preview.vertices[i*3].p+preview.vertices[i*3+1].p+preview.vertices[i*3+2].p)/3.f,direction);};return depthAt(a)<depthAt(b);});
   auto original=preview.vertices;for(std::size_t j=0;j<order.size();++j)for(int k=0;k<3;++k)preview.vertices[j*3+k]=original[order[j]*3+k];}
  glUniformMatrix4fv(glGetUniformLocation(program,"mvp"),1,GL_FALSE,glm::value_ptr(mvp));glUniform1i(glGetUniformLocation(program,"tiles"),tiles);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,material);
  glBufferData(GL_ARRAY_BUFFER,preview.vertices.size()*sizeof(PreviewVertex),preview.vertices.data(),GL_STREAM_DRAW);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  // Alpha-cutout geometry writes depth, translucent texels retain their alpha.
  glEnable(GL_BLEND);glDrawArrays(GL_TRIANGLES,0,GLsizei(preview.vertices.size()));glDisable(GL_BLEND);
  glReadPixels(0,0,S,S,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  for(std::size_t a=0;a<pixels.size();a+=4)if(pixels[a+3])for(int c=0;c<3;++c)pixels[a+c]=static_cast<unsigned char>(std::min(255,int(pixels[a+c])*255/int(pixels[a+3])));
  const int cell=itemDefinition(item).spriteIndex;for(int y=0;y<S;++y)std::copy_n(pixels.data()+(S-1-y)*S*4,S*4,atlas.data()+((cell/cols*S+y)*cols*S+cell%cols*S)*4);
  coverage<<id<<','<<cell<<",\""<<itemDefinition(item).displayName<<"\","<<preview.category<<','<<unsigned(blockType(preview.block))<<','<<blockProperties(preview.block)<<','<<preview.vertices.size()<<','<<ms(t)<<'\n';
 }
 raw(output/"thumbnails-128.rgba",cols*S,rows*S,atlas);
 glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteProgram(program);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&sourceTex);glDeleteTextures(1,&texture);glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fb);glEnable(GL_CULL_FACE);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Thumbnail OpenGL error");
 std::ofstream timing(output/"generation.txt");timing<<"source_resolution=128\noutput_resolution=64\ngeneration_ms="<<ms(started)<<'\n';std::cout<<"Generated "<<int(Item::Count)-1<<" thumbnails in "<<ms(started)<<" ms\n";return 0;
}
int IconTest::ui(Game& g,const std::filesystem::path& output,const std::filesystem::path& baseline){
 std::filesystem::create_directories(output);glfwSwapInterval(0);g.settings_.renderDistance=4;
 auto selected=WorldLibrary::create("Isolated icon gallery",991,GameMode::Creative,true);g.playSavedWorld(selected);
 auto& w=*g.world_;auto& inv=*g.inventory_;
 for(int x=-16;x<=48;x+=16)for(int z=-16;z<=32;z+=16)w.prepareSpawnTerrain({float(x),300,float(z)});
 w.fillBlocks({-16,299,-16},{48,299,32},Block::Stone);w.fillBlocks({-16,300,-16},{48,306,32},Block::Air);
 const Item display[]={Item::Grass,Item::Log,Item::Stone,Item::Cobblestone,Item::StoneBricks,Item::MossyCobblestone,Item::PolishedGranite,Item::PolishedDiorite,Item::PolishedAndesite,Item::DeepslateBricks,Item::OakStairs,Item::OakSlab,Item::OakFence,Item::OakFenceGate,Item::OakTable,Item::OakChair,Item::GlassPane,Item::Bed,Item::Barrel,Item::FlowerPot,Item::Lantern,Item::Campfire,Item::Scaffold,Item::SandstoneStairs};
 for(int i=0;i<24;++i){auto b=itemToBlock(display[i]);if(auto d=buildingDefinition(b))if(d->shape==BuildingShape::Fence || d->shape==BuildingShape::Wall || d->shape==BuildingShape::Pane)b=blockState(b,32|128);int x=(i%8)*4,z=(i/8)*4;w.setBlockInternal(x,300,z,b,true,false);if(b==Block::Bed)w.setBlockInternal(x,300,z-1,blockState(b,16),true,false);}
 for(int cx=-1;cx<=3;++cx)for(int cz=-1;cz<=2;++cz){auto* c=w.findChunk(cx,cz);if(!c)continue;w.computeSunlight(*c);w.markDirty(cx,cz,true);}
 g.setGameMode(GameMode::Spectator);g.player_->teleport({29,308,22});{const auto d=g.player_->lookDirection();g.player_->setMouseSensitivity(1);g.player_->addMouseMovement(-135.f-glm::degrees(std::atan2(d.z,d.x)),-20.f-glm::degrees(std::asin(d.y)));}g.timing_.worldTime=WorldTime::DefaultSeconds;g.fullbright_=true;g.showDebug_=false;
 auto capture=[&](const std::string& name){g.input_.setCursorCaptured(true);int tw,th;glfwGetWindowSize(g.window_,&tw,&th);glfwSetCursorPos(g.window_,tw*.72,th*.52);if(g.ui_.state()==GameState::Inventory || g.ui_.state()==GameState::CraftingTable){g.ui_.recipeBook_.entries.clear();for(int r=0;r<std::min(20,int(craftingRecipes().size()));++r)g.ui_.recipeBook_.entries.push_back(r);}g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path latest;for(const auto& file:std::filesystem::directory_iterator("screenshots"))if(latest.empty()||file.last_write_time()>std::filesystem::last_write_time(latest))latest=file.path();if(latest.empty())throw std::runtime_error("Missing game screenshot");std::filesystem::copy_file(latest,output/(name+".bmp"),std::filesystem::copy_options::overwrite_existing);};
 auto start=Clock::now();while(ms(start)<3500){glfwPollEvents();w.updateStreaming(g.player_->cameraPosition(),16);g.renderFrame(.008f);}
 capture("world-gallery");
 for(int i=0;i<24;++i){const glm::vec3 target((i%8)*4+.5f,300.5f,(i/8)*4+.5f);const auto camera=target+glm::normalize(glm::vec3(1,.81649658f,-1))*4.5f;g.player_->teleport(camera-glm::vec3(0,Player::EyeHeight,0));const auto d=g.player_->lookDirection();g.player_->addMouseMovement(135.f-glm::degrees(std::atan2(d.z,d.x)),-30.f-glm::degrees(std::asin(d.y)));capture("world-item-"+std::to_string(int(display[i])));}
 g.player_->teleport({29,308,22});{const auto d=g.player_->lookDirection();g.player_->addMouseMovement(-135.f-glm::degrees(std::atan2(d.z,d.x)),-20.f-glm::degrees(std::asin(d.y)));}
 g.setGameMode(GameMode::Creative);inv.clear();
 for(int part=0;part<4;++part){inv.add(Item(int(Item::DiamondHelmet)+part),1,Inventory::maxDurability(Item(int(Item::DiamondHelmet)+part))/2);inv.equipFromSlot(0);}inv.add(Item::Shield,1,120);inv.equipFromSlot(0);
 for(auto item:{Item::OakStairs,Item::StoneBricksStairs,Item::GlassPane,Item::IronSword,Item::DiamondPickaxe,Item::Bread,Item::FilledMap,Item::Carrot,Item::OakBoat,Item::OakDoor,Item::FlowerPot,Item::SpruceFenceGate,Item::Lantern,Item::Diamond,Item::StoneWall,Item::MossyCobblestone,Item::BirchPlanks,Item::DarkOakPlanks,Item::Campfire,Item::Scaffold})inv.add(item,std::min(37,Inventory::maxStack(item)),Inventory::maxDurability(item)>0?Inventory::maxDurability(item)/2:0);
 for(auto size:{glm::ivec2(1280,720),glm::ivec2(1920,1080),glm::ivec2(2560,1440)}){
  glfwSetWindowSize(g.window_,size.x,size.y);int W=0,H=0;for(int attempt=0;attempt<12;++attempt){glfwPollEvents();glfwGetFramebufferSize(g.window_,&W,&H);if(W==size.x&&H==size.y)break;std::this_thread::sleep_for(std::chrono::milliseconds(16));}
  if(W!=size.x||H!=size.y){
   std::cout<<"Window resolution limited to "<<W<<'x'<<H<<"; validating native UI at "<<size.x<<'x'<<size.y<<" in an offscreen target\n";
   W=size.x;H=size.y;GLuint fb,texture;glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("UI resolution FBO");glViewport(0,0,W,H);
   auto shot=[&](const std::string& name){auto dir=output/"offscreen";Screenshot::saveBmp(W,H,dir.string());std::filesystem::path latest;for(const auto& file:std::filesystem::directory_iterator(dir))if(latest.empty()||file.last_write_time()>std::filesystem::last_write_time(latest))latest=file.path();std::filesystem::copy_file(latest,output/(name+"-offscreen.bmp"),std::filesystem::copy_options::overwrite_existing);};
   auto clear=[] {glClearColor(.16f,.18f,.20f,1);glClear(GL_COLOR_BUFFER_BIT);};
   for(int page=0;page<4;++page){clear();g.renderer_->renderCreativeInventory(W,H,inv,0,0,page);shot("creative-"+std::to_string(W)+"-page-"+std::to_string(page+1));}
   RecipeBookView book;for(int i=0;i<std::min(20,int(craftingRecipes().size()));++i)book.entries.push_back(i);
   clear();g.renderer_->renderInventory(W,H,inv,false,book,0,0);shot("inventory-"+std::to_string(W));
   inv.setCreativeCursor(Item::StoneBricksStairs);clear();g.renderer_->renderInventory(W,H,inv,false,book,W*.72,H*.52);shot("drag-"+std::to_string(W));inv.takeCursor(false);
   clear();g.renderer_->renderInventory(W,H,inv,true,book,0,0);shot("crafting-"+std::to_string(W));
   ChestData chest;for(int i=0;i<27;++i)chest.slots[i]={Item(206+i),32};clear();g.renderer_->renderChest(W,H,inv,chest,0,0);shot("barrel-"+std::to_string(W));
   FurnaceData furnace;furnace.input={Item::IronOre,16};furnace.fuel={Item::Coal,12};furnace.output={Item::IronIngot,7};clear();g.renderer_->renderFurnace(W,H,inv,furnace,0,0);shot("furnace-"+std::to_string(W));
   glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fb);continue;
  }
  g.ui_.openInventory();for(int page=0;page<4;++page){g.ui_.creativePage_=page;capture("creative-"+std::to_string(W)+"-page-"+std::to_string(page+1));}
  g.setGameMode(GameMode::Survival);capture("inventory-"+std::to_string(W));
  inv.setCreativeCursor(Item::StoneBricksStairs);capture("drag-"+std::to_string(W));inv.takeCursor(false);
  g.ui_.openCraftingTable({0,300,0});
  inv.returnCraftingItems();for(const auto& r:craftingRecipes())if(r.output==Item::OakStairs){for(int i=0;i<9;++i)if(!r.cells[i].empty()){inv.setCreativeCursor(r.cells[i].group==IngredientGroup::Planks?Item::Planks:r.cells[i].item);auto rect=UiLayout::craftingSlot(i,W,H);inv.handleInventoryClick(rect.x+5,rect.y+5,W,H,false,true);inv.takeCursor(false);}break;}
  capture("crafting-"+std::to_string(W));
  w.setBlock(1,300,0,Block::Barrel);auto* chest=w.chestAt({1,300,0},true);for(int i=0;i<27;++i)chest->slots[i]={Item(206+i),std::min(32,Inventory::maxStack(Item(206+i)))};g.ui_.openChest({1,300,0});capture("barrel-"+std::to_string(W));
  w.setBlock(2,300,0,Block::Furnace);auto* furnace=w.furnaceAt({2,300,0},true);furnace->input={Item::IronOre,16};furnace->fuel={Item::Coal,12};furnace->output={Item::IronIngot,7};g.ui_.openFurnace({2,300,0});capture("furnace-"+std::to_string(W));
  g.setGameMode(GameMode::Creative);
 }
 g.ui_.closeGameplayInterface(inv);glfwSetWindowSize(g.window_,1280,720);glfwPollEvents();glViewport(0,0,1280,720);
 const GLuint current=g.renderer_->itemTexture_;std::ofstream bench(output/"ui-benchmark.csv");bench<<"atlas,trial,operation,mean_ms,p95_ms,p99_ms,worst_ms\n";
 for(int trial=0;trial<3;++trial)for(bool old:trial%2?std::vector<bool>{false,true}:std::vector<bool>{true,false}){
  GLuint temporary=0;auto loading=Clock::now();glGenTextures(1,&temporary);
  const auto newAtlas=baseline.parent_path().parent_path().parent_path().parent_path()/"assets/item_icons_expansion.rgba";
  std::ifstream f(old?baseline:newAtlas,std::ios::binary);int width=0,height=0;f.read(reinterpret_cast<char*>(&width),4);f.read(reinterpret_cast<char*>(&height),4);if(width!=640||height!=2240)throw std::runtime_error("Benchmark dimensions");std::vector<unsigned char> p(width*height*4);f.read(reinterpret_cast<char*>(p.data()),p.size());if(!f)throw std::runtime_error("Benchmark atlas read");glBindTexture(GL_TEXTURE_2D,temporary);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,p.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,old?GL_NEAREST:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,old?GL_NEAREST:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  g.renderer_->itemTexture_=temporary;glFinish();bench<<(old?"v2160":"v2161")<<','<<trial<<",atlas_load,"<<ms(loading)<<",0,0,0\n";
  for(int operation=0;operation<3;++operation){std::vector<double> samples;for(int frame=-20;frame<200;++frame){glClearColor(.14f,.16f,.18f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);auto t=Clock::now();if(operation==0){g.ui_.openInventory();g.renderer_->renderInventory(1280,720,inv,false,g.ui_.recipeBook(),0,0);}else g.renderer_->renderCreativeInventory(1280,720,inv,0,0,operation==2?(frame+20)%4:2);glFinish();if(frame>=0)samples.push_back(ms(t));}std::sort(samples.begin(),samples.end());double total=0;for(double t:samples)total+=t;bench<<(old?"v2160":"v2161")<<','<<trial<<','<<(operation==0?"open_inventory":operation==1?"creative_stationary":"creative_page_switch")<<','<<total/samples.size()<<','<<samples[190]<<','<<samples[198]<<','<<samples.back()<<'\n';}
  g.renderer_->itemTexture_=current;glDeleteTextures(1,&temporary);
 }
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("UI preview GL error");std::cout<<"UI previews: 3 resolutions, 4 catalog pages, equipment/cursor/crafting/barrel/furnace; asset A/B 3 repeats passed\n";return 0;
}
int IconTest::run(int argc,char** argv){if(argc<4)throw std::runtime_error("--icons-export output source-art.rgba");Game g;char app[]="icon-validation";char* args[]={app};auto start=Clock::now();if(!g.initialize(1,args))throw std::runtime_error("Icon fixture initialization");std::cout<<"initialize_ms="<<ms(start)<<'\n';int result=std::string(argv[1])=="--icons-export"?generate(g,argv[2],argv[3]):ui(g,argv[2],argv[3]);g.shutdown();return result;}
