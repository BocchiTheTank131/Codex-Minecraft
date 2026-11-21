    static void underwaterComparison(Game& g,const std::filesystem::path& out){
        g.settings_.renderDistance=8;g.settings_.simulationDistance=8;g.settings_.effectQuality=1;g.settings_.weatherQuality=2;
        g.settings_.weatherCycle=false;g.settings_.musicVolume=0;g.settings_.vsync=false;g.renderer_->setEffectQuality(1);
        g.playSavedWorld(WorldLibrary::create("Survival immersion fixture",20260917,GameMode::Survival,false));
        g.saveOnExit_=false;g.showDebug_=false;g.fullbright_=false;
        auto& w=*g.world_;glm::vec3 feet(-767.5f,60,256.5f);w.prepareSpawnTerrain(feet);g.player_->teleport(feet);
        g.timing_.previousFrame=glfwGetTime();g.updateSimulation(1.f/60);
        check(g.player_->isSwimming(),"survival_immersion_uses_actual_physics");
        auto eye=g.player_->cameraPosition();auto cell=glm::ivec3(glm::floor(eye));check(isWater(w.getBlock(cell.x,cell.y,cell.z)),"survival_camera_below_water");
        g.player_->yaw_=180;g.player_->pitch_=-8;g.timing_.worldTime=300;
        glfwSetWindowSize(g.window_,1280,720);glfwPollEvents();glfwSwapInterval(0);
        auto warm=glfwGetTime();while(glfwGetTime()-warm<2){w.updateStreaming(g.player_->cameraPosition(),8);g.renderFrame(0);}
        std::ofstream(out/"camera.csv")<<"x,y,z,yaw,pitch,time\n"<<eye.x<<','<<eye.y<<','<<eye.z<<",180,-8,300\n";
        glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(1280,720,(out/"underwater-survival").string()),"native_survival_immersion_capture");glReadBuffer(GL_BACK);
        check(glGetError()==GL_NO_ERROR,"survival_immersion_GL_valid");
    }
