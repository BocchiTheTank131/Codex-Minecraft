    static void atmosphere(Game& g,const std::filesystem::path& out){
        g.settings_.renderDistance=8;g.settings_.simulationDistance=8;
        g.settings_.weatherCycle=false;g.settings_.musicVolume=0;g.settings_.vsync=false;
        if(Persistence::enabled())g.playSavedWorld(WorldLibrary::create("Cosmetic source fixture",20260917,GameMode::Creative,false));
        else{g.seed_=20260917;g.creativeMode_=true;g.createWorldAndSystems();g.ui_.resumeGame();}
        g.saveOnExit_=false;g.showDebug_=false;auto& w=*g.world_;
        auto plan=naturalPlan(w);std::vector<glm::ivec3> sources;
        for(auto road:plan->roads){
            glm::ivec3 p(road.x,road.y+1,road.z);w.prepareSpawnTerrain(glm::vec3(p));
            if(!isSolid(w.getBlock(p.x,p.y-1,p.z))||w.getBlock(p.x,p.y,p.z)!=Block::Air)continue;
            bool separate=true;for(auto other:sources)if(glm::distance(glm::vec3(p),glm::vec3(other))<2)separate=false;
            if(!separate)continue;
            const Block lights[]={Block::Torch,Block::Lantern,Block::Campfire};
            w.setBlock(p.x,p.y,p.z,lights[sources.size()]);sources.push_back(p);if(sources.size()==3)break;
        }
        check(sources.size()==3,"natural_supported_light_sources");
        auto source=sources[0];auto camera=glm::vec3(source)+glm::vec3(.5f,.01f,3.5f);
        g.player_->teleport(camera);g.player_->flying_=true;g.timing_.worldTime=900;
        auto direction=glm::normalize(glm::vec3(source)+glm::vec3(.5f,.65f,.5f)-g.player_->cameraPosition());
        g.player_->yaw_=glm::degrees(std::atan2(direction.z,direction.x));g.player_->pitch_=glm::degrees(std::asin(direction.y));
        glfwSetWindowSize(g.window_,1280,720);glfwPollEvents();glfwSwapInterval(0);
        auto warm=glfwGetTime();while(glfwGetTime()-warm<3){w.updateStreaming(g.player_->cameraPosition(),8);g.renderFrame(0);}
        std::ofstream result(out/"source-points.csv");result<<"quality,points\n";
        // One percent leaves the integer dust budget at zero. Any emitted points
        // here are the verified light-source halos, not random dust or combat.
        GLuint highPoints=0;
        for(int q=0;q<3;++q){
            g.renderer_->clearParticles();g.renderer_->setEffectQuality(q);g.renderer_->setParticlePercent(1);
            for(int i=0;i<24;++i)g.renderer_->updateAtmospherics(.2f,w,g.player_->cameraPosition(),900);
            check(g.renderer_->cosmeticParticleCount()==0,"source_fixture_has_no_dust");
            g.renderFrame(0);
            GLuint query;glGenQueries(1,&query);glBeginQuery(GL_PRIMITIVES_GENERATED,query);
            g.renderer_->renderParticles(g.player_->viewMatrix(),glm::perspective(glm::radians(g.settings_.fov),1280.f/720,.1f,256.f),32);
            glEndQuery(GL_PRIMITIVES_GENERATED);GLuint points=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&points);glDeleteQueries(1,&query);
            if(q==2)highPoints=points;
            result<<q<<','<<points<<'\n';check(q==0?points==0:points>0&&points<=unsigned(q==1?4:16),"depth_tested_source_points_quality_budget");
            g.renderFrame(0);glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(1280,720,(out/("lights-q"+std::to_string(q))).string()),"native_source_capture");glReadBuffer(GL_BACK);
            check(glGetError()==GL_NO_ERROR,"source_points_GL_valid");
        }
        w.setBlock(source.x,source.y,source.z,Block::Candle);
        for(int i=0;i<24;++i)g.renderer_->updateAtmospherics(.2f,w,g.player_->cameraPosition(),900);
        GLuint query;glGenQueries(1,&query);glBeginQuery(GL_PRIMITIVES_GENERATED,query);
        g.renderer_->renderParticles(g.player_->viewMatrix(),glm::perspective(glm::radians(g.settings_.fov),1280.f/720,.1f,256.f),32);
        glEndQuery(GL_PRIMITIVES_GENERATED);GLuint remaining=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&remaining);glDeleteQueries(1,&query);
        check(remaining+1==highPoints,"replaced_emissive_block_removes_old_halo");
        check(glGetError()==GL_NO_ERROR,"replacement_source_GL_valid");
        g.renderer_->setParticlePercent(100);g.renderer_->setEffectQuality(2);
    }
