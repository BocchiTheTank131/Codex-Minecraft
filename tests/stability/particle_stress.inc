    static void particleStress(Game& g,const std::filesystem::path& out){
        glfwSetWindowSize(g.window_,1280,720);glfwPollEvents();glfwSwapInterval(0);
        int width=0,height=0;glfwGetFramebufferSize(g.window_,&width,&height);check(width==1280&&height==720,"particle_fixture_viewport");
        auto view=glm::lookAt(glm::vec3(0,2,6),glm::vec3(0,1,0),glm::vec3(0,1,0));
        auto projection=glm::perspective(glm::radians(70.f),1280.f/720,.1f,64.f);
        g.renderer_->setParticlePercent(100);g.renderer_->setEffectQuality(2);
        std::ofstream csv(out/"frames.csv");csv<<"bursts,frame,cpu_ms,gpu_ms,points\n";
        for(int bursts:{0,32,128}){
            g.renderer_->clearParticles();for(int i=0;i<bursts;++i)g.renderer_->spawnExplosionParticles({0,1,0});
            g.renderer_->updateParticles(.12f); // Identical fixed snapshot in both builds.
            GLuint timer,primitives;glGenQueries(1,&timer);glGenQueries(1,&primitives);GLuint points=0;
            for(int frame=0;frame<180;++frame){
                glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glClearColor(.08f,.10f,.14f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                glBeginQuery(GL_TIME_ELAPSED,timer);if(frame==0)glBeginQuery(GL_PRIMITIVES_GENERATED,primitives);
                auto start=glfwGetTime();g.renderer_->renderParticles(view,projection,64);double cpu=(glfwGetTime()-start)*1000;
                if(frame==0)glEndQuery(GL_PRIMITIVES_GENERATED);glEndQuery(GL_TIME_ELAPSED);
                GLuint64 gpu=0;glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&gpu);if(frame==0)glGetQueryObjectuiv(primitives,GL_QUERY_RESULT,&points);
                csv<<bursts<<','<<frame<<','<<cpu<<','<<double(gpu)/1e6<<','<<points<<'\n';check(glGetError()==GL_NO_ERROR,"combat_particle_GL_valid");
            }
            check(bursts==0?points==0:points>0,"combat_particle_drawn");
            glReadBuffer(GL_BACK);check(Screenshot::saveBmp(width,height,(out/("particles-"+std::to_string(bursts))).string()),"native_combat_capture");
            glDeleteQueries(1,&timer);glDeleteQueries(1,&primitives);
        }
        g.renderer_->clearParticles();g.saveOnExit_=false;
    }
