    // Shared verbatim by the frozen v2.19.0 and current production-object drivers.
    // Controlled NPC load; natural terrain, fixed camera, no ambient spawning.
    static void benchmark(Game& g,const std::filesystem::path& out,int argc,char** argv){
        const int count=argc>3?std::stoi(argv[3]):30,rd=argc>4?std::stoi(argv[4]):8;
        const int fps=argc>5?std::stoi(argv[5]):60;const double duration=argc>6?std::stod(argv[6]):12;
        g.settings_.renderDistance=rd;g.settings_.simulationDistance=8;g.settings_.vsync=false;g.settings_.frameLimit=0;g.settings_.musicVolume=0;
        g.playSavedWorld(WorldLibrary::create("Village workload",20260917,GameMode::Survival,false));
        auto& w=*g.world_;auto plan=w.structures_.majorPlanForRegion(5,-1,[&](int x,int z){return w.structureTerrainAt(x,z);});
        if(plan.pieces.empty()||plan.roads.empty())throw std::runtime_error("Natural benchmark village absent");
        for(auto piece:plan.pieces)w.prepareSpawnTerrain({piece.x+.5f,piece.y+1.01f,piece.z+.5f});
        auto center=plan.pieces.front();g.player_->teleport(w.findSafeSpawnNear(center.x+6,center.z+6));
        auto look=glm::normalize(glm::vec3(center.x+.5f,center.y+2.f,center.z+.5f)-g.player_->cameraPosition());
        g.player_->yaw_=glm::degrees(std::atan2(look.z,look.x));g.player_->pitch_=glm::degrees(std::asin(look.y));g.setGameMode(GameMode::Creative);
        g.survival_=std::make_unique<SurvivalWorld>(w.seed());g.survival_->setWeather(g.weather_.get());g.survival_->spawnAccumulator_=-100000;
        for(int i=0;i<count;++i){auto r=plan.roads[(i*7)%plan.roads.size()];SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType::Villager;a.position=a.home={r.x+.5f,r.y+1.01f,r.z+.5f};a.health=20;a.persistent=true;a.age=20;a.thinkTimer=i*.037f;g.survival_->animals_.push_back(a);}
        auto warm=glfwGetTime();while(glfwGetTime()-warm<6){g.timing_.previousFrame=glfwGetTime();g.updateSimulation(1.f/60);g.renderFrame(1.f/60);}
        g.pollSave(true);g.timing_.lastSave=glfwGetTime()+100000;
        std::ofstream csv(out/"frames.csv");csv<<"frame,phase,wall_ms,work_ms,simulation_ms,npc_ai_last_tick_ms,gpu_ms,loaded,triangles,ram_mb,nodes_last_tick,queries_last_tick\n";
        GLuint queries[16]{};bool pending[16]{};glGenQueries(16,queries);unsigned frame=0;auto start=glfwGetTime(),previous=start;
        while(glfwGetTime()-start<duration){auto begin=glfwGetTime();double t=begin-start;const int phase=std::min(3,int(t/(duration/4)));g.timing_.worldTime=phase==1?800:phase==3?520:100;
            if(phase==2)g.weather_->set(WeatherType::Rain,3600);else g.weather_->set(WeatherType::Clear,3600);
            glfwPollEvents();g.timing_.previousFrame=begin;auto update=glfwGetTime();g.updateSimulation(1.f/fps);auto simulation=(glfwGetTime()-update)*1000;
            double gpu=-1;auto q=frame%16;if(pending[q]){GLint ready=0;glGetQueryObjectiv(queries[q],GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 ns=0;glGetQueryObjectui64v(queries[q],GL_QUERY_RESULT,&ns);gpu=ns/1e6;pending[q]=false;}}
            bool issue=!pending[q];if(issue)glBeginQuery(GL_TIME_ELAPSED,queries[q]);
#ifdef VILLAGE_NEW_BUILD
            // Busy UI includes live offer changes every 30 frames. The baseline
            // shows its existing inventory during the matching UI phase.
            if(phase==3){auto& a=g.survival_->animals_.front();if(a.villagerId){g.villageOpen_=true;g.villageNpc_=a.villagerId;g.villageTab_=0;if(frame%30==0){g.inventory_->add(Item::Cobblestone,64);g.inventory_->add(Item::Wheat,64);g.inventory_->add(Item::IronIngot,16);w.villages.trade(a.villagerId,0,*g.inventory_,g.villageMessage_);}}}else g.villageOpen_=false;
#else
            if(phase==3&&g.ui_.state()!=GameState::Inventory)g.ui_.openInventory();
#endif
            // End the timing query inside renderFrame, before SwapBuffers.
            g.optimizationGpuQueryActive_=issue;g.renderFrame(1.f/fps);if(issue)pending[q]=true;
            auto work=(glfwGetTime()-begin)*1000;PROCESS_MEMORY_COUNTERS mem{};GetProcessMemoryInfo(GetCurrentProcess(),&mem,sizeof(mem));
            unsigned nodes=0,searches=0;
#ifdef VILLAGE_NEW_BUILD
            nodes=g.survival_->villageNodesLastTick_;searches=g.survival_->villageQueriesLastTick_;if(nodes>512||searches>2)throw std::runtime_error("Unbounded village pathfinding");
#endif
            const auto target=begin+1.0/fps;while(glfwGetTime()<target)std::this_thread::sleep_for(std::chrono::microseconds(100));auto end=glfwGetTime();
            csv<<frame++<<','<<phase<<','<<(end-previous)*1000<<','<<work<<','<<simulation<<','<<g.survival_->aiMilliseconds_<<','<<gpu<<','<<w.loadedChunkCount()<<','<<w.renderedTriangleCount()<<','<<mem.WorkingSetSize/1048576.0<<','<<nodes<<','<<searches<<'\n';previous=end;
        }
        glDeleteQueries(16,queries);
#ifdef VILLAGE_NEW_BUILD
        g.villageOpen_=false;
#endif
        std::ofstream save(out/"save.csv");save<<"capture_ms,total_ms,snapshot_bytes,encode_ms,commit_ms\n";
        for(int i=0;i<3;++i){auto t=glfwGetTime();if(!g.saveAll(true))throw std::runtime_error("Benchmark snapshot rejected");auto capture=(glfwGetTime()-t)*1000;auto status=g.pendingSaveStatus_;g.pollSave(true);save<<capture<<','<<(glfwGetTime()-t)*1000<<','<<status->snapshotBytes<<','<<status->encodingMilliseconds<<','<<status->commitMilliseconds<<'\n';}
        g.saveOnExit_=false;
    }
