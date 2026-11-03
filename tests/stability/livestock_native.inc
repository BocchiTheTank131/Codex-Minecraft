    static void livestock(Game& g,const std::filesystem::path& out){
        World w(991,24);auto p=plan(w);flat(w,*p);
        const auto c=glm::vec3(p->pieces.front().x+.5f,81.01f,p->pieces.front().z+.5f);
        Player player(c+glm::vec3(12,0,12));Inventory inventory;
        for(auto type:{SurvivalWorld::AnimalType::Pig,SurvivalWorld::AnimalType::Cow}){
            SurvivalWorld mobs(991);SurvivalWorld::Animal a;a.type=type;a.position=a.home=c;a.health=10;a.grounded=true;a.heading={0,1};a.thinkTimer=100;
            mobs.animals_.push_back(a);auto parts=mobs.renderCuboids();
            check(parts.size()>=28&&parts.size()<=48,"livestock_distinct_detailed_model");
            for(auto box:parts)check(box.instanced&&std::isfinite(box.center.x)&&box.size.x>0&&box.size.y>0&&box.size.z>0,"livestock_shared_batch_valid_parts");
            auto adult=mobs.mobOutline(0);mobs.animals_[0].age=-100;auto baby=mobs.mobOutline(0);
            check(glm::length(baby.size-adult.size*.55f)<.0001f,"calf_and_piglet_scale_about_feet");mobs.animals_[0].age=0;
            check(mobs.targetedAnimal(c+glm::vec3(0,.6f,3),{0,0,-1},5)==0,"livestock_front_target");
            check(mobs.targetedAnimal(c+glm::vec3(0,type==SurvivalWorld::AnimalType::Cow?1.05f:.77f,3),{0,0,-1},5)==0,"livestock_head_target");
            check(mobs.targetedAnimal(c+glm::vec3(3,.6f,0),{-1,0,0},5)==0,"livestock_side_target");
            for(int fps:{15,30,60,120}){
                auto moving=a;
                for(int frame=0;frame<fps*2;++frame)mobs.updateAnimal(moving,1.f/fps,w,player,inventory,1);
                check(glm::distance(moving.position,a.position)>1&&moving.modelMotion>.5f&&moving.gait>0,"stride_uses_actual_simulated_movement");
                auto phase=moving.gait;moving.heading={0,0};moving.thinkTimer=100;
                for(int frame=0;frame<fps;++frame)mobs.updateAnimal(moving,1.f/fps,w,player,inventory,1);
                check(moving.gait==phase&&moving.modelMotion<.001f,"idle_does_not_walk_or_slide_legs");
                check(std::isfinite(moving.modelYaw)&&moving.modelClock>0,"livestock_pose_all_frame_rates_finite");
            }
            float yaw=0,phase=1,motion=1,clock=0;
            for(int frame=0;frame<60;++frame)advanceLivestockPose(yaw,phase,motion,clock,{0,0},{1,0},1.f/60,true,type==SurvivalWorld::AnimalType::Cow);
            check(phase==1&&motion<.001f&&std::abs(yaw-1.5707963f)<.001f,"blocked_steps_stop_and_turn_smoothly");
            advanceLivestockPose(yaw,phase,motion,clock,{0,0},{0,0},.5f,true,false);check(std::isfinite(yaw)&&phase==1,"short_stall_keeps_pose_valid");
            mobs.animals_[0].hurtFlash=.2f;auto hurt=mobs.renderCuboids();check(hurt.front().color.r>parts.front().color.r,"hurt_tint_applies_to_livestock");
            mobs.animals_[0].hurtFlash=0;mobs.animals_[0].deathTimer=.325f;auto dying=mobs.renderCuboids();check(glm::length(dying.front().size-parts.front().size*.5f)<.0001f,"existing_death_scale_preserved");mobs.animals_[0].deathTimer=0;
            if(Persistence::enabled()){
                auto one=(out/(type==SurvivalWorld::AnimalType::Cow?"cow-a.vxm":"pig-a.vxm")).string();auto two=one+"-pose";
                check(mobs.save(one,991),"livestock_save_before_pose");mobs.animals_[0].modelYaw=1;mobs.animals_[0].modelClock=12;mobs.animals_[0].modelMotion=.8f;mobs.animals_[0].gait=2;
                check(mobs.save(two,991),"livestock_save_after_pose");
                auto bytes=[](const std::string& name){std::ifstream f(name,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
                check(bytes(one)==bytes(two),"animation_does_not_change_saved_bytes");SurvivalWorld restored(991);check(restored.load(one,991)&&restored.animals_.front().type==type,"existing_livestock_save_reload");
            }
        }
        for(bool cow:{false,true}){
            auto base=glm::ivec3(c)+glm::ivec3(4,0,4);
            auto cross=[&](Block obstacle,bool tall=false){for(int z=-1;z<5;++z)for(int x=-1;x<=1;++x)for(int y=0;y<4;++y)w.setBlockInternal(base.x+x,base.y+y,base.z+z,Block::Air,false,false);w.setBlockInternal(base.x,base.y,base.z+1,obstacle,false,false);if(tall)w.setBlockInternal(base.x,base.y+1,base.z+1,Block::Stone,false,false);glm::vec3 at=glm::vec3(base)+glm::vec3(.5f,.01f,.5f),v(0);bool ground=true;
                for(int i=0;i<360;++i){at=moveLivestock(w,at,{0,0,.75f},1.f/60,cow,v,ground);check(!w.aabbIntersectsSolid(at+glm::vec3(-.299f,.002f,-.299f),at+glm::vec3(.299f,cow?1.179f:.889f,.299f)),"livestock_body_never_enters_solid");}return at;};
            check(cross(Block::Stone).z>base.z+2,"livestock_jumps_one_block_step");
            check(cross(Block::OakSlab).z>base.z+2,"livestock_crosses_half_slab");
            check(cross(Block::OakFence).z<base.z+1.2f,"livestock_stays_inside_fences");
            check(cross(Block::Stone,true).z<base.z+1,"livestock_cannot_walk_through_wall");
            auto chunks=std::move(w.chunks_);glm::vec3 at=c,v(0);bool ground=true;check(moveLivestock(w,at,{1,0,0},.2f,cow,v,ground)==at,"livestock_unloaded_terrain_barrier");w.chunks_=std::move(chunks);
        }
        // Native renderer turntables: real generated geometry, no postprocessing.
        for(int pose=0;pose<6;++pose){
            SurvivalWorld models(991);
            for(int i=0;i<4;++i){SurvivalWorld::Animal a;a.type=i%2?SurvivalWorld::AnimalType::Pig:SurvivalWorld::AnimalType::Cow;a.position={i*1.5f,0,0};a.health=10;a.age=i>=2?-100.f:0;a.grounded=true;a.modelYaw=pose==1?1.5707963f:pose==2?3.1415926f:0;a.gait=1.2f;a.modelMotion=pose==3?1:0;a.modelClock=2;a.hurtFlash=pose==4?.2f:0;a.deathTimer=pose==5?.325f:0;models.animals_.push_back(a);}
            glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,1280,720);glEnable(GL_DEPTH_TEST);glClearColor(.13f,.18f,.22f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            auto camera=glm::vec3(2.1f,1.5f,6.4f);g.renderer_->renderEntities(models.renderCuboids(),glm::lookAt(camera,glm::vec3(2.1f,.55f,0),{0,1,0}),glm::perspective(glm::radians(46.f),1280.f/720,.1f,50.f),300,32);
            check(glGetError()==GL_NO_ERROR,"native_livestock_batch_GL_valid");glReadBuffer(GL_BACK);check(Screenshot::saveBmp(1280,720,(out/("livestock-pose-"+std::to_string(pose))).string()),"livestock_native_capture");
        }
        for(int count:{1,10,30,60}){
            std::vector<RenderCuboid> parts;for(int i=0;i<count;++i)appendLivestockModel(parts,{float(i%10)*1.4f,0,float(i/10)*1.4f},1,.3f,.7f,.75f,2,i%2==0,true,0);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);g.renderer_->renderEntities(parts,glm::lookAt(glm::vec3(6,6,18),glm::vec3(6,.5f,4),glm::vec3(0,1,0)),glm::perspective(glm::radians(46.f),1280.f/720,.1f,50.f),300,32);
            check(glGetError()==GL_NO_ERROR&&parts.size()<=std::size_t(count)*48,"livestock_cohort_shared_batch_1_10_30_60");
        }
        if(!Persistence::enabled())return;
        auto saved=WorldLibrary::create("Livestock visual fixture",20260917,GameMode::Creative,false);g.playSavedWorld(saved);auto& world=*g.world_;
        auto spawn=world.findSafeSpawnNear(0,0);world.prepareSpawnTerrain(spawn);g.survival_=std::make_unique<SurvivalWorld>(world.seed());g.survival_->spawnAccumulator_=-100000;
        for(int i=0;i<2;++i){SurvivalWorld::Animal a;a.type=i?SurvivalWorld::AnimalType::Pig:SurvivalWorld::AnimalType::Cow;a.position=a.home=world.findSafeSpawnNear(int(spawn.x)+i*2,int(spawn.z));a.health=10;a.heading={0,1};a.thinkTimer=10;g.survival_->animals_.push_back(a);}
        g.player_->teleport(world.findSafeSpawnNear(int(spawn.x)+1,int(spawn.z)+6));g.showDebug_=false;g.timing_.worldTime=300;
        auto aim=[&]{auto target=(g.survival_->animals_[0].position+g.survival_->animals_[1].position)*.5f+glm::vec3(0,.6f,0);auto d=glm::normalize(target-g.player_->cameraPosition());g.player_->yaw_=glm::degrees(std::atan2(d.z,d.x));g.player_->pitch_=glm::degrees(std::asin(d.y));};
        glfwSetWindowSize(g.window_,1280,720);
        for(int frame=0;frame<120;++frame){glfwPollEvents();g.timing_.previousFrame=glfwGetTime();g.updateSimulation(1.f/60);aim();g.renderFrame(1.f/60);}
        glReadBuffer(GL_FRONT);check(Screenshot::saveBmp(1280,720,(out/"livestock-in-world").string()),"native_livestock_world_capture");glReadBuffer(GL_BACK);g.saveOnExit_=false;
    }
