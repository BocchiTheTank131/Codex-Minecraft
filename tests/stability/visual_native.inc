    static void visual(Game& g,const std::filesystem::path& out,int argc,char** argv){
        int quality=argc>3?std::stoi(argv[3]):1,rd=argc>4?std::stoi(argv[4]):8;
        g.settings_.effectQuality=quality;g.settings_.weatherQuality=quality+1;g.settings_.renderDistance=rd;g.settings_.simulationDistance=8;g.settings_.frameLimit=0;g.settings_.vsync=false;g.settings_.musicVolume=0;g.settings_.weatherCycle=false;g.settings_.snowAccumulation=false;
        g.renderer_->setEffectQuality(quality);g.renderer_->setParticlePercent(100);
        g.playSavedWorld(WorldLibrary::create("Atmosphere fixture",20260917,GameMode::Creative,false));g.showDebug_=false;g.player_->flying_=true;g.saveOnExit_=false;
        glfwSetWindowSize(g.window_,1280,720);glfwSwapInterval(0);
        std::ofstream(out/"gl.txt")<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<" / "<<glGetString(GL_VERSION)<<"\n";
        auto& w=*g.world_;glm::ivec2 ocean(0),mountain(0),snow(0);int tallest=-64;bool sea=false,cold=false;
        for(int z=-2048;z<=2048;z+=256)for(int x=-2048;x<=2048;x+=256){int h=w.terrainHeight(x,z);if(h>tallest){tallest=h;mountain={x,z};}if(!sea&&h<54){ocean={x,z};sea=true;}auto name=w.biomeNameAt(x,z);if(!cold&&(name.find("SNOW")!=std::string::npos||name.find("FROZEN")!=std::string::npos)){snow={x,z};cold=true;}}
        auto villagePlan=naturalPlan(w);check(!villagePlan->roads.empty(),"native_village_plan");auto road=villagePlan->roads.front();glm::ivec2 village(road.x,road.z);
        struct Scene{const char* name;glm::ivec2 xz;float time,pitch;WeatherType weather;bool submerged=false;};
        const Scene scenes[]={{"forest",{0,0},300,0,WeatherType::Clear},{"sunrise",mountain,1140,8,WeatherType::Clear},{"sunset",village,520,4,WeatherType::Clear},{"night",mountain,900,25,WeatherType::Clear},{"cave",village,300,0,WeatherType::Clear},{"village-night",village,900,0,WeatherType::Clear},{"rain",village,300,0,WeatherType::Rain},{"storm",village,300,12,WeatherType::Thunder},{"snow",snow,300,0,WeatherType::Rain},{"ocean",ocean,300,-8,WeatherType::Clear},{"underwater",ocean,300,-8,WeatherType::Clear,true},{"foliage",{0,0},300,-10,WeatherType::Clear}};
        std::ofstream csv(out/"frames.csv");csv<<"scene,quality,rd,frame,cpu_ms,gpu_ms,loaded,triangles,particles,ram_mb\n";
        std::ofstream resources(out/"resources.csv");resources<<"scene,terrain_gpu_capacity_mb,terrain_gpu_used_mb,opaque_draws,water_draws,queued_chunks\n";
        std::ofstream cameras(out/"cameras.csv");cameras<<"scene,x,y,z,yaw,pitch,time\n";
        int sceneIndex=0;for(const auto& scene:scenes){if(rd==64&&sceneIndex++>0)break;
            float h=float(w.terrainHeight(scene.xz.x,scene.xz.y));glm::vec3 camera(scene.xz.x+.5f,h+4,scene.xz.y+.5f);
            if(scene.xz==ocean)camera.y=scene.submerged?60.f:68.f;
            w.prepareSpawnTerrain(camera);
            if(std::string(scene.name)=="cave"){
                bool found=false;int best=-1;glm::ivec3 chosen;
                for(int y=50;y>-40;--y)for(int z=-8;z<=8;++z)for(int x=-8;x<=8;++x){auto p=glm::ivec3(camera)+glm::ivec3(x,0,z);p.y=y;
                    if(w.getBlock(p.x,p.y,p.z)!=Block::Air||w.getBlock(p.x,p.y+1,p.z)!=Block::Air||w.getBlock(p.x,p.y+2,p.z)!=Block::Air||!isSolid(w.getBlock(p.x,p.y-1,p.z)))continue;
                    int space=0;for(int step=1;step<=10;++step){if(!w.hasLoadedChunkAt(p.x-step,p.z)||w.getBlock(p.x-step,p.y+1,p.z)!=Block::Air)break;++space;}
                    if(space>best){best=space;chosen=p;found=true;}
                }
                if(found){camera=glm::vec3(chosen)+glm::vec3(.5f,.01f,.5f);w.setBlock(chosen.x+1,chosen.y,chosen.z,Block::Torch);}
                check(found,"natural_cave_fixture_found");
            }
            g.player_->teleport(camera);g.player_->flying_=true;g.player_->yaw_=std::string(scene.name)=="sunrise"?0:180;g.player_->pitch_=scene.pitch;g.timing_.worldTime=scene.time;
            g.weather_=std::make_unique<Weather>(991);g.survival_->setWeather(g.weather_.get());
            std::istringstream weatherState("WEATHER1 "+std::to_string(int(scene.weather))+" 1000 "+(scene.weather==WeatherType::Clear?"0":"1")+" "+(scene.weather==WeatherType::Thunder?"1":"0")+" 0 16 991\nFIRES1 0\n");check(g.weather_->read(weatherState),"reproducible_weather_state");
            g.weather_->update(1.f/30,w,*g.player_,*g.survival_,*g.sounds_,g.settings_);
            if(std::string(scene.name)=="storm")g.weather_->strike(camera+glm::vec3(-12,0,0),w,*g.player_,*g.survival_,*g.sounds_);
            auto warm=glfwGetTime();while(glfwGetTime()-warm<2){w.updateStreaming(g.player_->cameraPosition(),rd);g.renderFrame(0);}
            cameras<<scene.name<<','<<camera.x<<','<<camera.y<<','<<camera.z<<','<<g.player_->yaw_<<','<<scene.pitch<<','<<scene.time<<'\n';
            GLuint query=0;glGenQueries(1,&query);
            for(int i=0;i<90;++i){glfwPollEvents();auto start=glfwGetTime();glBeginQuery(GL_TIME_ELAPSED,query);
#ifdef VISUAL_POLISH_CANDIDATE
                g.renderer_->updateAtmospherics(1.f/60,w,g.player_->cameraPosition(),scene.time);g.renderer_->updateParticles(1.f/60);check(g.renderer_->cosmeticParticleCount()<=std::size_t(quality==0?0:quality==1?24:64),"bounded_cosmetic_particles");
#endif
                g.renderFrame(1.f/60);glEndQuery(GL_TIME_ELAPSED);double cpu=(glfwGetTime()-start)*1000;
                GLuint64 gpu=0;glGetQueryObjectui64v(query,GL_QUERY_RESULT,&gpu); // fixture-only completion, excluded from CPU work
                PROCESS_MEMORY_COUNTERS memory{};GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));
                csv<<scene.name<<','<<quality<<','<<rd<<','<<i<<','<<cpu<<','<<double(gpu)/1e6<<','<<w.loadedChunkCount()<<','<<w.renderedTriangleCount()<<','<<g.weatherRenderer_->particleCount()<<','<<memory.WorkingSetSize/1048576.0<<'\n';
                check(glGetError()==GL_NO_ERROR,"native_visual_GL_valid");
            }
            const auto mem=w.terrainMemory();resources<<scene.name<<','<<mem.capacityBytes/1048576.0<<','<<mem.usedBytes/1048576.0<<','<<w.opaqueSubmissionCount()<<','<<w.waterSubmissionCount()<<','<<w.pendingChunkCount()<<'\n';
            glDeleteQueries(1,&query);glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(1280,720,(out/scene.name).string()),"native_visual_capture");glReadBuffer(GL_BACK);
        }
        if(quality==1&&rd==8){
            for(auto size:{glm::ivec2(1280,720),glm::ivec2(1920,1080),glm::ivec2(2560,1440)})for(int aa:{0,2,4})for(int scale=0;scale<4;++scale){
                glfwSetWindowSize(g.window_,size.x,size.y);glfwPollEvents();g.settings_.antiAliasingSamples=aa;g.settings_.uiScale=scale;g.fullbright_=scale==3;g.ui_.openInventory();
                g.renderFrame(0);check(glGetError()==GL_NO_ERROR,"visual_AA_scale_fullbright_valid");
                glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(size.x,size.y,(out/("ui-"+std::to_string(size.x)+"-aa"+std::to_string(aa)+"-s"+std::to_string(scale))).string()),"native_UI_capture");glReadBuffer(GL_BACK);
            }
            g.ui_.closeGameplayInterface(*g.inventory_);g.fullbright_=false;
            for(auto mode:{GameMode::Creative,GameMode::Spectator}){g.setGameMode(mode);g.renderFrame(0);check(glGetError()==GL_NO_ERROR,"visual_game_mode_valid");glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(2560,1440,(out/(mode==GameMode::Creative?"creative":"spectator")).string()),"native_game_mode_capture");glReadBuffer(GL_BACK);}
#ifdef VISUAL_POLISH_CANDIDATE
            const auto loaded=w.loadedChunkCount();std::ostringstream beforeWeather;g.weather_->write(beforeWeather);
            for(int q=0;q<3;++q){g.renderer_->setEffectQuality(q);for(int i=0;i<200;++i){g.renderer_->updateAtmospherics(.2f,w,g.player_->cameraPosition(),300);g.renderer_->updateParticles(.2f);check(g.renderer_->cosmeticParticleCount()<=std::size_t(q==0?0:q==1?24:64),"quality_particle_bound");}}
            g.renderer_->setEffectQuality(0);g.renderer_->updateAtmospherics(.2f,w,g.player_->cameraPosition(),300);check(g.renderer_->cosmeticParticleCount()==0,"low_clears_cosmetics");std::ostringstream afterWeather;g.weather_->write(afterWeather);check(beforeWeather.str()==afterWeather.str()&&loaded==w.loadedChunkCount(),"cosmetics_no_weather_or_streaming_mutation");
#endif
        }
    }
