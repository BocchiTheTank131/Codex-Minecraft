#include "Game.h"
#include "World.h"
#include "Renderer.h"
#ifdef VOXEL_RENDER_OPTIMIZATION
#include "TerrainLod.h"
#endif
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
void require(bool good,const char* message) {if(!good)throw std::runtime_error(message);}
void image(const std::string& name,const std::vector<unsigned char>& pixels,int width,int height) {
    std::ofstream file(name,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)for(int x=0;x<width;++x)file.write(reinterpret_cast<const char*>(pixels.data()+4*(y*width+x)),3);
}
}
void World::writeOptimizationChunkHashes(const std::string& path) const {
    std::ofstream file(path),timings(path+".cpu.csv");file<<"seed,version,x,z,bytes,hash\n";
    timings<<"seed,version,x,z,generation_ms\n";
    for(unsigned seed:{123456789U,20260917U,42U})for(unsigned version=1;version<=22;++version) {
        World probe(seed,version);
        std::vector<glm::ivec2> coordinates{glm::ivec2(0,0),glm::ivec2(-1,-1),glm::ivec2(1,-1),glm::ivec2(-3,2),
            glm::ivec2(17,-29),glm::ivec2(64,64),glm::ivec2(-128,123),glm::ivec2(377,-911),glm::ivec2(-3011,2741)};
        if(version==22){coordinates.clear();for(int i=0;i<32;++i)coordinates.push_back({(i%8)*173-606,(i/8)*197-295});}
        for(const auto coordinate:coordinates) {
            const auto start=std::chrono::steady_clock::now();
            const auto data=probe.generateChunkData(coordinate.x,coordinate.y);
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            timings<<seed<<','<<version<<','<<coordinate.x<<','<<coordinate.y<<','<<ms<<'\n';
            std::uint64_t hash=14695981039346656037ULL;
            for(auto block:data.blocks){hash^=std::uint8_t(block);hash*=1099511628211ULL;}
            file<<seed<<','<<version<<','<<coordinate.x<<','<<coordinate.y<<','<<data.blocks.size()<<','<<hash<<'\n';
        }
    }
}
#ifdef VOXEL_RENDER_OPTIMIZATION
bool World::runOptimizationSelfTest(std::string& report) {
    try {
        require(TerrainPacking::selfTest(),"Packed attributes round trip");
        // Exact complete-attribute indexing, including a discontinuity in each
        // input field and meshes crossing the 16-bit range.
        std::vector<VoxelVertex> source;
        for(int q=0;q<17000;++q)for(int corner:{0,1,2,0,2,3}) {
            source.push_back({{float(q),float(corner),-.0005f},{.0005f,float(corner)},
                {0,1,0},float(q%16)/15.f,float(q%16)/15.f,float(49+q%16)/64.f,float(q%100)});
        }
        source[3].uv.x+=.0005f;source[10].ao=.875f;
        std::vector<PackedTerrainVertex> packed;std::vector<std::uint32_t> indices;
        TerrainPacking::indexQuads(source,packed,indices);
        require(packed.size()>65536 && indices.size()==source.size(),"32-bit index fallback fixture");
        for(std::size_t i=0;i<source.size();++i) {
            require(indices[i]<packed.size(),"Index overflow");
            const auto actual=TerrainPacking::unpack(packed[indices[i]]);
            require(!std::memcmp(&actual,&source[i],48),"Indexed triangle attributes/order changed");
        }
        for(unsigned bytes:{2U,4U}) {
            TerrainBufferPage page(4096,bytes);std::size_t a=0,b=0,c=0;
            require(page.allocate(512,a)&&page.allocate(1024,b)&&page.allocate(768,c),"Paired page allocation");
            page.release(b,1024);page.release(a,512);page.release(c,768);
            require(page.allocated==0 && page.freeRanges.size()==1 && page.freeRanges[0].count==4096,"Page free-range merging");
        }
        generate(2,{.5f,120.f,.5f});prepareSpawnTerrain({.5f,120.f,.5f});
        // Synthetic canopy/decor scene: each reduced mesh is an ordered subset;
        // all terrain and transparent triangles retain full precision.
        auto input=captureMeshInput(0,0);
        const int span=22;input.meshHeight=88;input.minimumY=64;input.lod=0;
        input.blocks.assign(span*span*24,Block::Air);
        input.packedLight.assign(input.blocks.size(),0xf0);
        const auto put=[&](int x,int y,int z,Block block){input.blocks[(y*span+z+3)*span+x+3]=block;};
        for(int z=0;z<16;++z)for(int x=0;x<16;++x)put(x,0,z,Block::Stone);
        for(int z=3;z<7;++z)for(int x=3;x<7;++x)for(int y=3;y<6;++y)put(x,y,z,Block::Leaves);
        put(9,1,9,Block::TallGrass);put(10,1,9,Block::Water);put(11,1,9,Block::Glass);put(12,1,9,Block::Torch);
        const auto full=buildMesh(input);
        for(int level:{1,2}) {
            input.lod=level;const auto reduced=buildMesh(input);
            require(reduced.water.size()==full.water.size() && !std::memcmp(reduced.water.data(),full.water.data(),full.water.size()*48),"LOD changed transparent triangles");
            std::size_t cursor=0;
            for(const auto& vertex:reduced.opaque) {
                while(cursor<full.opaque.size() && std::memcmp(&vertex,&full.opaque[cursor],48))++cursor;
                require(cursor<full.opaque.size(),"LOD moved/changed retained geometry");++cursor;
            }
            require(reduced.opaque.size()<full.opaque.size(),"LOD did not reduce far detail");
        }
        require(TerrainLod::desired(50,64,3,2)==0 && TerrainLod::desired(10000,64,0,2)==0,"Near/off LOD rules");
        const float threshold=std::max(192.f,32*16*.48f);
        require(TerrainLod::desired(threshold+10,32,3,0)==0 && TerrainLod::desired(threshold+10,32,3,1)==1,"LOD hysteresis");
        setTerrainLod(3);updateTerrainLod({10000.f,120.f,10000.f});setTerrainLod(0);
        for(const auto& entry:chunks_)require(entry.second->requestedLod==0,"Off left a stale LOD request");
        require(glGetError()==GL_NO_ERROR,"Paired terrain buffer GL error");
        if(!runStreamingSelfTest(report))throw std::runtime_error(report);
        for(const auto coordinate:{glm::ivec2(0,0),glm::ivec2(-1,-1),glm::ivec2(1,1)}) {
            const auto snapshot=captureMeshInput(coordinate.x,coordinate.y);
            MeshOutput scratch;std::vector<PackedTerrainVertex> plain,indexed;std::vector<std::uint32_t> order;
            double buildMs=0,packMs=0,indexMs=0;std::size_t logical=0,unique=0;
            for(int repeat=0;repeat<30;++repeat) {
                const auto start=std::chrono::steady_clock::now();
                scratch=buildMesh(snapshot,std::move(scratch));
                const auto built=std::chrono::steady_clock::now();
                plain.clear();for(const auto& vertex:scratch.opaque)plain.push_back(TerrainPacking::pack(vertex));
                const auto packedAt=std::chrono::steady_clock::now();
                TerrainPacking::indexQuads(scratch.opaque,indexed,order);
                const auto indexedAt=std::chrono::steady_clock::now();
                if(repeat>=5) {
                    buildMs+=std::chrono::duration<double,std::milli>(built-start).count();
                    packMs+=std::chrono::duration<double,std::milli>(packedAt-built).count();
                    indexMs+=std::chrono::duration<double,std::milli>(indexedAt-packedAt).count();
                }
                logical=scratch.opaque.size();unique=indexed.size();
            }
            std::cout<<"Mesh CPU fixed snapshot "<<coordinate.x<<','<<coordinate.y<<": raw "<<buildMs/25
                <<" ms; pack-only "<<packMs/25<<" ms; pack+index "<<indexMs/25<<" ms; "<<logical<<" to "<<unique<<" vertices\n";
        }
        report="Packing/index attributes, 16/32-bit page lifecycle and merging, full terrain/transparent LOD preservation, hysteresis; "+report;
        return true;
    } catch(const std::exception& e){report=e.what();return false;}
}
#endif

void Game::runOptimizationSmoke() {
    namespace fs=std::filesystem;fs::create_directories(optimizationSmoke_);
    std::string report="Original v2.13.0 reference";
    world_->writeOptimizationChunkHashes(optimizationSmoke_+"/chunk-hashes.csv");
#ifdef VOXEL_RENDER_OPTIMIZATION
    if(!world_->runOptimizationSelfTest(report))throw std::runtime_error(report);
    require(renderer_->validateTerrainPacking(),"Legacy/packed/indexed shader pixels");
#endif
    constexpr int width=640,height=360;
    GLuint framebuffer,color,depth;glGenFramebuffers(1,&framebuffer);glGenTextures(1,&color);glGenRenderbuffers(1,&depth);
    glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Occlusion/water fixture FBO");
    GLuint msaaFramebuffer,msaaColor,msaaDepth;glGenFramebuffers(1,&msaaFramebuffer);
    glGenRenderbuffers(1,&msaaColor);glGenRenderbuffers(1,&msaaDepth);
    glBindFramebuffer(GL_FRAMEBUFFER,msaaFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER,msaaColor);glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,GL_RGBA8,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,msaaColor);
    glBindRenderbuffer(GL_RENDERBUFFER,msaaDepth);glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,GL_DEPTH_COMPONENT24,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,msaaDepth);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"4x MSAA fixture FBO");
    const auto scenes=world_->optimizationScenes();constexpr const char* names[]={"plains","forest","mountains","cave","ocean","structures"};
    unsigned queryFrames=0,culled=0;
    const auto projection=glm::perspective(glm::radians(70.f),float(width)/height,.1f,800.f);
    std::vector<unsigned char> reference(width*height*4),msaaReference(reference.size()),actual(reference.size());
    for(int scene=0;scene<6;++scene) {
        const auto origin=scenes[scene];
#ifdef VOXEL_RENDER_OPTIMIZATION
        world_->setTerrainLod(0);
#endif
        world_->generate(8,origin);world_->prepareSpawnTerrain(origin);
        const double start=glfwGetTime();
        do {world_->updateStreaming(origin,8);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        while((world_->pendingChunkCount()||world_->queuedMeshRebuildCount()||world_->pendingCpuMeshCount()||world_->completedMeshCount())&&glfwGetTime()-start<30);
        require(!world_->pendingChunkCount()&&!world_->queuedMeshRebuildCount()&&!world_->pendingCpuMeshCount()&&!world_->completedMeshCount(),"Pixel fixture streaming did not settle");
        auto camera=origin+glm::vec3(0,scene==4?4:1.65f,0);
        for(int pose=0;pose<4;++pose) {
            if(pose==1)camera+=glm::vec3(3,0,-3);
            if(pose==2) {
                world_->setBlock(int(std::floor(camera.x))+2,int(std::floor(camera.y)),int(std::floor(camera.z))-3,Block::Glass);
                const double editStart=glfwGetTime();
                do {world_->updateStreaming(origin,8);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
                while((world_->queuedMeshRebuildCount()||world_->pendingCpuMeshCount()||world_->completedMeshCount())&&glfwGetTime()-editStart<10);
                require(!world_->queuedMeshRebuildCount()&&!world_->pendingCpuMeshCount()&&!world_->completedMeshCount(),"Edit meshes failed to recover");
            }
            if(pose==3) {
#ifdef VOXEL_RENDER_OPTIMIZATION
                world_->setTerrainMemoryBudget(1);
#endif
                camera.y+=10;
            }
            const auto view=glm::lookAt(camera,camera+glm::normalize(glm::vec3(pose==1?1.f:.3f,-.15f,-1)),glm::vec3(0,1,0));
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);
            const auto draw=[&](int mode,bool batch,std::vector<unsigned char>& pixels,bool msaa) {
                glBindFramebuffer(GL_FRAMEBUFFER,msaa?msaaFramebuffer:framebuffer);
                glViewport(0,0,msaa?width/2:width,msaa?height/2:height);
#ifdef VOXEL_RENDER_OPTIMIZATION
                renderer_->setOcclusionMode(mode);world_->setWaterBatchingForValidation(batch);
#endif
                glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
                glClearColor(.5f,.7f,.9f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                renderer_->renderWorld(*world_,view,projection,camera,300.f,.5f);
                if(msaa) {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER,msaaFramebuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);
                    glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
                    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
                }
                glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            };
            draw(0,false,reference,false);draw(0,false,msaaReference,true);
            for(int frame=0;frame<20;++frame) {
                const bool msaa=frame>=10;draw(2,true,actual,msaa);
                require(actual==(msaa?msaaReference:reference),"Occlusion/water batching changed visible pixels");
#ifdef VOXEL_RENDER_OPTIMIZATION
                queryFrames+=renderer_->occlusionQueryCount()>0;culled+=renderer_->occlusionCulledCount();
                require(renderer_->occlusionQueryCount()<=8,"Occlusion query budget");
#endif
            }
            if(pose==0)image(optimizationSmoke_+"/"+names[scene]+".ppm",reference,width,height);
        }
    }
#ifdef VOXEL_RENDER_OPTIMIZATION
    require(queryFrames>0,"Occlusion fixtures issued no queries");
#endif
    require(glGetError()==GL_NO_ERROR,"Occlusion/water GL lifetime error");
#ifdef VOXEL_RENDER_OPTIMIZATION
    world_->setWaterBatchingForValidation(true);renderer_->setOcclusionMode(0);world_->setTerrainMemoryBudget(settings_.terrainMemoryBudget);
#endif
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    glDeleteFramebuffers(1,&msaaFramebuffer);glDeleteRenderbuffers(1,&msaaColor);glDeleteRenderbuffers(1,&msaaDepth);
    std::cout<<"Optimization smoke passed: "<<report<<"; 480 exact full-scene pixel comparisons (including 4x MSAA/viewport changes), "<<queryFrames<<" query frames, "<<culled<<" hidden chunk draws skipped.\n";
}
