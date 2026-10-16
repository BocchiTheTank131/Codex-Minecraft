#include "Game.h"
#include "Transportation.h"
#include "Player.h"
#include "World.h"
#include "Persistence.h"
#include "SaveFile.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#endif

bool Transportation::selfTest(World& w,std::string& report){
    auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    if(Persistence::enabled()){std::string saves;check(SaveFile::selfTest("save-tests",saves),"save stress");std::cout<<saves<<std::endl;}
    // Negative-coordinate basin with an east wall, dry platform, head clearance and a chunk boundary.
    for(int x=-32;x<=16;x+=16)for(int z=-32;z<=16;z+=16)w.prepareSpawnTerrain({float(x),80,float(z)});
    w.fillBlocks({-30,73,-30},{12,88,12},Block::Air);
    w.fillBlocks({-30,74,-30},{12,74,12},Block::Stone);
    w.fillBlocks({-30,75,-30},{12,77,12},Block::Water);
    w.fillBlocks({5,75,-30},{6,82,12},Block::Stone);
    SurvivalWorld mobs(991);Inventory inventory;
    for(int fps:{15,30,60,120,240}){
        Transportation t({-12,80,-12});check(t.placeBoat(w,Item::OakBoat,{-12.5f,77,-12.5f},0),"boat placement");
        Player player({-13.8f,78,-12.5f});const auto& hull=t.boats_[0];
        check(t.useBoat(player,w,hull.position+glm::vec3(0,1,0),{0,-1,0},4),"mount");
        PlayerInput input;input.enabled=true;input.moveForward=true;
        for(int i=0;i<fps*4;++i){const float dt=1.f/fps;t.update(dt,input,w,player,mobs);player.update(dt,input,w);}
        check(t.boats_[0].position.x<4.34f,"shore tunneling");
        check(std::abs(t.boats_[0].position.y-(77.875f-.23f))<.08f,"buoyancy");
        if(t.boats_[0].position.x<=1) throw std::runtime_error("boat acceleration fps="+std::to_string(fps)+" x="+std::to_string(t.boats_[0].position.x)+" y="+std::to_string(t.boats_[0].position.y)+" vx="+std::to_string(t.boats_[0].velocity.x));
        check(player.health()==20 && player.isRiding(),"rider health/seat");
        check(t.dismount(player,w)&&!player.isRiding(),"safe water/shore dismount");
        t.boats_[0].velocity={30,0,0};t.update(.5f,{},w,player,mobs);
        check(t.boats_[0].position.x<4.34f&&t.boatSteps<=32,"spike tunneling/bounded steps");
        t.boats_[0].velocity={32,-18,0};t.update(1.f,{},w,player,mobs);check(t.boatSteps<=32&&t.boats_[0].position.x<4.34f,"one-second swept catch-up");
    }
    {
        Transportation traffic({-12,80,-12});check(traffic.placeBoat(w,Item::OakBoat,{-15,77,-16},0)&&traffic.placeBoat(w,Item::BirchBoat,{-9,77,-16},0),"traffic placement");
        Player rider({-15,79,-16});check(traffic.useBoat(rider,w,traffic.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"traffic mount");
        PlayerInput drive;drive.enabled=true;drive.moveForward=true;
        for(int i=0;i<300;++i)traffic.update(1.f/60,drive,w,rider,mobs);
        check(traffic.boats_[0].position.x<traffic.boats_[1].position.x-1.33f,"boat to boat collision");
        rider.teleport({-20,80,-20});traffic.update(.02f,{},w,rider,mobs);check(!traffic.mounted()&&!rider.isRiding(),"teleport detaches rider");
        SurvivalWorld animals(991);check(animals.summonMob("cow",{-8,77.6f,-12}),"entity fixture");Transportation entity({-15,79,-12});check(entity.placeBoat(w,Item::OakBoat,{-15,77,-12},0),"entity boat");
        Player driver({-15,79,-12});check(entity.useBoat(driver,w,entity.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"entity mount");
        for(int i=0;i<300;++i)entity.update(1.f/60,drive,w,driver,animals);
        check(entity.boats_[0].position.x< -9.04f,"entity collision volume");
        check(!entity.placeBoat(w,Item::OakBoat,{10000,77,10000},0),"unloaded terrain placement");
    }
    {
        Transportation corner({4.2f,79,-18});check(!corner.placeBoat(w,Item::OakBoat,{4.2f,77,-18},.785398f),"rotated corner placement rejects wall");
        check(corner.placeBoat(w,Item::OakBoat,{4.2f,77,-18},0),"aligned wall placement");Player rider({4.2f,79,-18});check(corner.useBoat(rider,w,corner.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"corner mount");PlayerInput turn;turn.enabled=true;turn.moveRight=true;
        for(int i=0;i<180;++i){corner.update(1.f/60,turn,w,rider,mobs);const auto& b=corner.boats_[0];check(b.position.x+std::abs(std::cos(b.heading))*.67f+std::abs(std::sin(b.heading))*.64f<=5.001f,"rotation clips corner");}
        w.fillBlocks({-21,75,-25},{-21,80,-10},Block::Stone);w.fillBlocks({-18,75,-25},{-18,80,-10},Block::Stone);
        Transportation channel({-19,79,-24});check(channel.placeBoat(w,Item::SpruceBoat,{-19,77,-24},1.5707963f),"two-block channel placement");Player sailor({-19,79,-24});check(channel.useBoat(sailor,w,channel.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"channel mount");PlayerInput forward;forward.enabled=true;forward.moveForward=true;
        for(int i=0;i<150;++i)channel.update(1.f/60,forward,w,sailor,mobs);check(channel.boats_[0].position.z> -16,"two-block channel movement");
        channel.boats_[0].position={-2,87,-22};channel.boats_[0].velocity={0,-18,0};sailor.setRidePosition(channel.boats_[0].position+glm::vec3(0,.25f,0),channel.boats_[0].velocity);
        for(int i=0;i<240;++i)channel.update(1.f/60,{},w,sailor,mobs);check(std::abs(channel.boats_[0].position.y-77.645f)<.1f,"fall into water buoyancy recovery");
    }
    {
        Transportation exits({-12,80,-12});check(exits.placeBoat(w,Item::OakBoat,{-12,77,-12},0),"exit hull fixture");
        check(exits.placeBoat(w,Item::BirchBoat,{-9.65f,77,-11.25f},.7853982f),"rotated neighboring exit hull");
        Player sailor({-12,79,-12});check(exits.useBoat(sailor,w,exits.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"exit mount");
        check(exits.dismount(sailor,w)&&!exits.intersectsBox(sailor.aabbMinimum(),sailor.aabbMaximum()),"safe dismount must reject actual rotated hull, not center radius");
        check(sailor.position().y>=77.87f,"water exit prefers exposed surface over submerged floor");
    }
    {
        Transportation solid({-12,80,-12});check(solid.placeBoat(w,Item::OakBoat,{-12,77,-23},0),"solid player fixture");
        w.fillBlocks({-15,77,-26},{-9,77,-17},Block::Stone);solid.boats_[0].position.y=78;
        for(int fps:{15,60,240}){Player walker({-12,78,-19});walker.setTransportation(&solid);PlayerInput input;input.enabled=true;input.moveForward=true;
            for(int i=0;i<fps*3;++i){walker.update(1.f/fps,input,w);check(!solid.intersectsBox(walker.aabbMinimum(),walker.aabbMaximum()),"player intersects hull while walking");}
            if(solid.intersectsBox(walker.aabbMinimum(),walker.aabbMaximum())||walker.position().z<=-22.07f)throw std::runtime_error("walking hull fps="+std::to_string(fps)+" z="+std::to_string(walker.position().z)+" y="+std::to_string(walker.position().y)+" inside="+std::to_string(solid.intersectsBox(walker.aabbMinimum(),walker.aabbMaximum())));
            Player landing(solid.boats_[0].position+glm::vec3(0,2,0));landing.setTransportation(&solid);
            for(int i=0;i<fps*2;++i)landing.update(1.f/fps,{},w);
            check(std::abs(landing.position().y-(solid.boats_[0].position.y+.5401f))<.01f,"player lands on hull");
        }
        w.fillBlocks({-15,77,-26},{-9,77,-17},Block::Water);
    }
    Transportation t({-12,80,-12});for(int n=0;n<6;++n)check(t.placeBoat(w,Item(int(Item::OakBoat)+n),{-25.f+n*3.f,77,-22.f},0),"six wood boats");
    check(!t.placeBoat(w,Item::OakBoat,{-25,77,-22},0),"boat overlap");
    {
        Transportation flow({-12,80,-12});
        w.fillBlocks({-26,77,-6},{-5,77,-2},Block::WaterFlow3);
        check(flow.placeBoat(w,Item::BirchBoat,{-23,77,-4},0),"flowing water placement");
        Player rider({-23,79,-4});check(flow.useBoat(rider,w,flow.boats_[0].position+glm::vec3(0,1,0),{0,-1,0},3),"flow mount");
        PlayerInput input;input.enabled=true;input.moveForward=true;input.moveRight=true;
        for(int i=0;i<120;++i)flow.update(1.f/60,input,w,rider,mobs);
        check(std::abs(flow.boats_[0].heading)>.8f,"steering");check(flow.boats_[0].position.y>77.25f,"flow buoyancy");
        input.moveRight=false;input.moveForward=false;input.moveBackward=true;
        for(int i=0;i<300;++i)flow.update(1.f/60,input,w,rider,mobs);
        const auto f=glm::vec3(std::cos(flow.boats_[0].heading),0,std::sin(flow.boats_[0].heading));
        check(glm::dot(flow.boats_[0].velocity,f)<0,"reverse acceleration");
    }
    {
        const auto before=mobs.renderItemSprites().size();Player breaker(t.boats_[0].position+glm::vec3(0,0,3));breaker.addMouseMovement(0,-150);
        check(t.breakBoat(breaker,inventory,mobs,w,5),"boat recovery");check(mobs.renderItemSprites().size()==before+1,"one boat drop operation");
        check(!t.breakBoat(breaker,inventory,mobs,w,5),"no duplicate boat recovery");
        check(t.placeBoat(w,Item::OakBoat,{-25,77,-22},0),"boat replacement");
    }

    {
        Transportation maximum({0,80,0});Player observer({-12,80,-12});observer.setSpectatorMode(true);
        for(int i=0;i<1024;++i)maximum.boats_.push_back({std::uint32_t(i+1),Item::OakBoat,{float(1000+i*3),77,float(-1000-i*3)},{},0,0});
        maximum.boats_[0].position={-12,77.645f,-12};maximum.nextBoat_=1025;
        maximum.update(.5f,{},w,observer,mobs);check(maximum.boatSteps<=32 && maximum.boatCollisionCandidates==0,"broadphase filters 1023 distant boats");
        check(!maximum.placeBoat(w,Item::OakBoat,{-25,77,-12},0),"boat limit");
        for(int i=0;i<128;++i)check(maximum.createMap(observer.position())!=0,"128 maps");
        check(!maximum.createMap(observer.position()),"map limit");
        for(auto& m:maximum.maps_)m.pixels.fill(0xff507050U); // Worst-case bounded 8 MiB survey plane.
        for(int i=0;i<64;++i)check(maximum.addWaypoint("Point",{float(i-32),70,float(i*10)}),"64 waypoints");
        check(!maximum.addWaypoint("Overflow",observer.position()),"waypoint limit");
        if(Persistence::enabled()){const auto start=std::chrono::steady_clock::now();check(maximum.save("save-tests/maximum.vtn",991),"maximum object save");std::cout<<"Maximum navigation serialization_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<std::endl;Transportation copy({0,0,0});check(copy.load("save-tests/maximum.vtn",991)&&copy.boats().size()==1024&&copy.waypoints().size()==64&&copy.maps_.size()==128,"maximum object reload");}
        std::cout<<"Bounded boats: 1024 saved, 1 active; collision candidates="<<maximum.boatCollisionCandidates<<" steps="<<maximum.boatSteps<<" main_ms="<<maximum.boatMilliseconds<<std::endl;
    }
    {
        Transportation compass({-22000,70,31000});Player traveler({180000,80,-90000});traveler.setSpectatorMode(true);
        for(int turn=0;turn<8;++turn){traveler.addMouseMovement(450,0);for(int i=0;i<120;++i)compass.update(1.f/120,{},w,traveler,mobs);
            const auto d=compass.originalSpawn()-traveler.position(),f=traveler.lookDirection();const float expected=std::atan2(d.z,d.x)-std::atan2(f.z,f.x);
            check(std::abs(std::remainder(compass.compassAngle-expected,6.2831853f))<.002f,"compass facing/wrap/distant accuracy");}
        traveler.teleport({-280000,90,310000});for(int i=0;i<120;++i)compass.update(1.f/120,{},w,traveler,mobs);
        const auto d=compass.originalSpawn()-traveler.position(),f=traveler.lookDirection();check(std::abs(std::remainder(compass.compassAngle-(std::atan2(d.z,d.x)-std::atan2(f.z,f.x)),6.2831853f))<.002f,"compass teleport settles");
    }
    {
        Transportation crowded({-12,80,-12});Player observer({-12,80,-12});observer.setSpectatorMode(true);
        for(int z=-14;z<=1;z+=3)for(int x=-14;x<=1;x+=3){
            if(crowded.placeBoat(w,Item::OakBoat,{float(x),77,float(z)},0))crowded.boats_.back().velocity={((x+14)/3)%2?1.5f:-1.5f,0,((z+14)/3)%2?1.3f:-1.3f};
        }
        check(crowded.boats_.size()>=25,"crowded water fixture");
        double crowdedTotal=0,crowdedWorst=0;
        for(int frame=0;frame<180;++frame){crowded.update(frame==50?.5f:1.f/60,{},w,observer,mobs);crowdedTotal+=crowded.boatMilliseconds;crowdedWorst=std::max(crowdedWorst,double(crowded.boatMilliseconds));
            for(std::size_t i=0;i<crowded.boats_.size();++i)for(std::size_t j=i+1;j<crowded.boats_.size();++j){const auto& a=crowded.boats_[i];const auto& b=crowded.boats_[j];
                check(std::abs(a.position.x-b.position.x)>=1.339f || std::abs(a.position.z-b.position.z)>=1.279f || std::abs(a.position.y-b.position.y)>=.539f,"crowded boats do not overlap");}
        }
        std::cout<<"Crowded boats: "<<crowded.boats_.size()<<" hulls, 180 updates including 500ms spike passed; average_ms="<<crowdedTotal/180<<" worst_ms="<<crowdedWorst<<std::endl;
    }
    {
        w.fillBlocks({-6,78,-9},{-2,w.maximumY()-1,-5},Block::Air);
        w.fillBlocks({-6,90,-9},{-2,90,-5},Block::Leaves);w.fillBlocks({-4,78,-7},{-4,89,-7},Block::Log);
        Player forestWalker({-3.5f,79,-6.5f}),caveWalker({-3.5f,65,-6.5f});Transportation forest(forestWalker.position());
        check(w.precipitationHeight(-4,-7)>forestWalker.position().y+5,"old precipitation gate rejects tall canopy fixture");
        const auto surfaceMap=forest.createMap(forestWalker.position()),caveMap=forest.createMap(caveWalker.position());
        const int pixel=(-7-forest.map(surfaceMap)->originZ)*128+(-4-forest.map(surfaceMap)->originX);
        for(int i=0;i<200;++i){forest.reveal(w,forestWalker,surfaceMap);forest.finishMapJob(true);if(forest.map(surfaceMap)->pixels[pixel])break;}
        check(forest.map(surfaceMap)->pixels[pixel]!=0,"walking beneath tall canopy surveys forest");
        for(int i=0;i<200;++i){forest.reveal(w,caveWalker,caveMap);forest.finishMapJob(true);}
        check(forest.map(caveMap)->pixels[pixel]==0,"underground forest does not reveal unseen surface");
        w.setBlock(-4,91,-7,Block::Snow);const auto snowMap=forest.createMap(forestWalker.position());
        for(int i=0;i<200;++i){forest.reveal(w,forestWalker,snowMap);forest.finishMapJob(true);if(forest.map(snowMap)->pixels[pixel])break;}
        check(forest.map(snowMap)->pixels[pixel]!=0,"snow-covered canopy surveys from ground");w.setBlock(-4,91,-7,Block::Air);
        const auto observed=forest.map(surfaceMap)->pixels;w.setBlock(-4,90,-7,Block::SnowBlock);
        check(forest.refreshMap(surfaceMap),"refresh known forest from below");
        for(int i=0;i<400&&forest.refreshing(surfaceMap);++i){forest.reveal(w,caveWalker,surfaceMap);forest.finishMapJob(true);}
        check(forest.map(surfaceMap)->pixels[pixel]!=observed[pixel],"known loaded terrain refreshes from below");
        for(int i=0;i<16384;++i)check(observed[i]||!forest.map(surfaceMap)->pixels[i],"underground refresh does not reveal unknown");
        w.fillBlocks({-6,78,-9},{-2,90,-5},Block::Air);
        std::cout<<"Map canopy regression: ground traversal reveals foliage; underground reveal stays hidden; known-only refresh updates from below passed"<<std::endl;
    }
    const auto id=t.createMap({-10,80,-10});const auto id2=t.createMap({-10,80,-10});check(id&&id2!=id,"map identity");
    for(auto pixel:t.map(id)->pixels)check(pixel==0,"map starts hidden");
    Player explorer({-12,79,-12});t.reveal(w,explorer,id);t.finishMapJob(true);
    check(std::count_if(t.map(id)->pixels.begin(),t.map(id)->pixels.end(),[](auto c){return c!=0;})>0,"loaded map survey");
    const auto beforeRefresh=t.map(id)->pixels;
    check(t.refreshMap(id)&&!t.refreshMap(id),"refresh deduplicated");
    // Recolor surveyed loaded pixels after a surface edit, never reveal unknown pixels.
    int edited=-1;for(int i=0;i<16384;++i)if(beforeRefresh[i]){edited=i;break;}
    check(edited>=0,"refresh surveyed fixture");
    const int editX=t.map(id)->originX+edited%128,editZ=t.map(id)->originZ+edited/128;
    const int editY=int(std::floor(w.precipitationHeight(editX,editZ)-.01f));w.setBlock(editX,editY,editZ,Block::SnowBlock);
    for(int n=0;n<400&&t.refreshing(id);++n){t.reveal(w,explorer,id);t.finishMapJob(true);}
    check(!t.refreshing(id)&&t.map(id)->pixels[edited]!=beforeRefresh[edited],"loaded map refresh updates surveyed edit");
    for(int i=0;i<16384;++i)check(beforeRefresh[i]||t.map(id)->pixels[i]==0,"refresh never reveals unknown");
    const auto unchangedRevision=t.map(id)->revision;check(t.refreshMap(id),"second refresh");
    for(int n=0;n<400&&t.refreshing(id);++n){t.reveal(w,explorer,id);t.finishMapJob(true);}
    check(t.map(id)->revision==unchangedRevision,"unchanged refresh avoids texture revision/upload");
    const auto zoom=t.zoomMap(id);check(zoom&&t.map(zoom)->scale==1,"map zoom");
    check(t.map(id2)->revision==0,"independent maps");
    check(t.addWaypoint("Harbor",{-1040,70,1280})&&t.renameWaypoint(0,"Port"),"waypoint names");check(t.target().x==-1040,"waypoint direction");
    check(inventory.add(Item::FilledMap,1,0,id)==0 && inventory.add(Item::FilledMap,1,0,id2)==0,"map inventory");
    check(inventory.slot(0).identity==id&&inventory.slot(1).identity==id2,"map separation");
    inventory.clickPlayerSlot(0,false);ItemStack chest;inventory.clickExternalSlot(chest,false);check(chest.identity==id&&inventory.slot(0).empty(),"map chest transfer");
    inventory.clickExternalSlot(chest,false);inventory.clickPlayerSlot(2,false);check(inventory.slot(2).identity==id,"map return");
    auto dropped=inventory.takeSlot(2,true);check(dropped.identity==id&&inventory.slot(2).empty(),"map drop identity");
    if(Persistence::enabled()){
        std::filesystem::create_directories("transport-tests");check(t.save("transport-tests/travel.vtn",991),"navigation save");
        Transportation loaded({0,0,0});check(loaded.load("transport-tests/travel.vtn",991),"navigation reload");
        check(loaded.boats_.size()==6&&loaded.map(id)->pixels==t.map(id)->pixels&&loaded.originalSpawn()==t.originalSpawn()&&loaded.waypoints()[0].name=="Port","roundtrip data");
        check(inventory.save("transport-tests/inventory.vxi",991),"inventory save");Inventory inv;check(inv.load("transport-tests/inventory.vxi",991)&&inv.slot(1).identity==id2,"inventory map reload");
        mobs.spawnDrop(explorer.position(),Item::FilledMap,1,0,{},0,id);
        check(mobs.save("transport-tests/drops.vxm",991),"dropped map save");SurvivalWorld drops(991);check(drops.load("transport-tests/drops.vxm",991)&&drops.renderItemSprites().size()==2,"dropped maps/boats reload");
        {
            std::ofstream legacy("transport-tests/inventory-v5.vxi",std::ios::binary);legacy.write("VXINV5\0",8);const std::uint32_t seed=991;const int selected=0;legacy.write(reinterpret_cast<const char*>(&seed),4);legacy.write(reinterpret_cast<const char*>(&selected),4);
            for(int i=0;i<51;++i){const std::uint8_t item=i?0:std::uint8_t(Item::IronSword);const std::int16_t count=i?0:1,durability=i?0:123;legacy.write(reinterpret_cast<const char*>(&item),1);legacy.write(reinterpret_cast<const char*>(&count),2);legacy.write(reinterpret_cast<const char*>(&durability),2);}legacy.close();
            Inventory migrated;check(migrated.load("transport-tests/inventory-v5.vxi",991)&&migrated.slot(0).durability==123&&migrated.slot(0).identity==0&&migrated.equipment(4).empty(),"legacy V5 migration");
        }
        for(int checkpoint=0;checkpoint<12;++checkpoint){
            const auto identity=t.createMap({float(checkpoint*10),80,-12});inventory.clear();inventory.add(Item::FilledMap,1,0,identity);
            {SaveFile::Checkpoint batch("transport-tests/save.checkpoint");check(t.save("transport-tests/travel-pair.vtn",991)&&inventory.save("transport-tests/inventory-pair.vxi",991)&&batch.commit(),"real map/inventory checkpoint");}
            Transportation pair({0,0,0});Inventory owned;check(pair.load("transport-tests/travel-pair.vtn",991)&&owned.load("transport-tests/inventory-pair.vxi",991)&&pair.map(owned.slot(0).identity)!=nullptr,"map references resolve after checkpoint/restart");
        }
        auto* container=w.chestAt({-10,79,-10});(void)container;
        w.setBlock(-10,79,-10,Block::Chest);w.chestAt({-10,79,-10},true)->slots[0]=dropped;
        check(w.saveWorld("transport-tests/world.vxw",explorer.position()),"chest world save");World copy(w.seed(),22);glm::vec3 spawn;check(copy.loadWorld("transport-tests/world.vxw",spawn),"chest reload");check(copy.chestAt({-10,79,-10})->slots[0].identity==id,"chest map identity reload");
    }else check(t.save("transport-demo-forbidden.vtn",991)&&!std::filesystem::exists("transport-demo-forbidden.vtn"),"demo stateless navigation");
    report="Transportation passed: six woods, 15/30/60/120/240 FPS, 500ms spike, shores, negative chunk boundary, buoyancy, mount/dismount, independent maps, fog/zoom, chest/drop identities, waypoints, versioned persistence/demo guards";return true;
}
void Game::runTransportationSmoke(){
    std::string report;saveOnExit_=false;
    auto previousWorld=std::move(world_);world_=std::make_unique<World>(991,22);
    if(!Transportation::selfTest(*world_,report))throw std::runtime_error(report);
    transportation_=std::make_unique<Transportation>(glm::vec3(-12,80,-12));
    player_=std::make_unique<Player>(glm::vec3(-12,79,-12));player_->setEquipment(inventory_.get());
    auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
    auto clearInput=[&]{input_.keys_.fill(false);input_.previousKeys_.fill(false);input_.mouseButtons_.fill(false);input_.previousMouseButtons_.fill(false);input_.typedCharacters_.clear();};
    ui_.resumeGame();clearInput();waypointName_.clear();waypointCoordinates_="-128 70 256";
    input_.keys_[GLFW_KEY_N]=true;check(navigationInput()&&navigationOpen_,"N opens navigation");
    input_.previousKeys_[GLFW_KEY_N]=true;input_.typedCharacters_="North Port";check(navigationInput()&&navigationOpen_&&waypointName_=="North Port","N in waypoint names");
    clearInput();input_.keys_[GLFW_KEY_ENTER]=true;navigationInput();check(transportation_->waypoints().size()==1&&transportation_->target().x==-128,"UI create coordinates");
    clearInput();waypointName_="Northern Harbor";input_.keys_[GLFW_KEY_LEFT_CONTROL]=input_.keys_[GLFW_KEY_ENTER]=true;navigationInput();check(transportation_->waypoints().size()==1&&transportation_->waypoints()[0].name==waypointName_,"UI rename without duplicate");
    clearInput();waypointCoordinates_="nan 70 0";input_.keys_[GLFW_KEY_ENTER]=true;navigationInput();check(transportation_->waypoints().size()==1,"invalid coordinate rejected");
    clearInput();input_.keys_[GLFW_KEY_DELETE]=true;navigationInput();check(transportation_->waypoints().size()==1,"deletion needs confirmation");
    clearInput();input_.keys_[GLFW_KEY_DELETE]=true;navigationInput();check(transportation_->waypoints().empty(),"confirmed deletion");
    clearInput();input_.keys_[GLFW_KEY_ESCAPE]=true;navigationInput();check(!navigationOpen_,"navigation Escape");
    clearInput();inventory_->clear();inventory_->add(Item::EmptyMap);interaction_.hasBlockTarget=false;input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();
    check(inventory_->selectedStack().item==Item::FilledMap&&inventory_->selectedStack().identity&&mapView_,"RMB map creation owns one map");const auto id=inventory_->selectedStack().identity;
    input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(mapView_&&inventory_->selectedStack().identity==id,"held RMB does not repeat use");
    inventory_->swapOffhand();clearInput();input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(inventory_->equipment(4).identity==id&&!mapView_,"offhand map use");
    inventory_->add(Item::Paper,8);clearInput();input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();
    check(inventory_->equipment(4).identity!=id&&transportation_->map(inventory_->equipment(4).identity)->scale==1&&inventory_->count(Item::Paper)==0,"zoom input consumes eight paper once");
    const auto zoomed=inventory_->equipment(4).identity;inventory_->add(Item::Paper,8);
    clearInput();input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();
    check(inventory_->equipment(4).identity==zoomed&&inventory_->count(Item::Paper)==8,"rapid zoom debounce preserves paper");
    mapView_=true;viewedMap_=zoomed;clearInput();input_.keys_[GLFW_KEY_F6]=true;navigationInput();
    check(transportation_->refreshing(zoomed)&&inventory_->count(Item::Paper)==8,"refresh input has no paper cost");
    viewedMap_=transportation_->createMap(player_->position());const auto previousJump=settings_.controls[std::size_t(ControlAction::Jump)];settings_.controls[std::size_t(ControlAction::Jump)]=GLFW_KEY_F6;
    clearInput();input_.keys_[GLFW_KEY_F6]=true;navigationInput();check(!transportation_->refreshing(viewedMap_),"refresh shortcut yields to custom controls");settings_.controls[std::size_t(ControlAction::Jump)]=previousJump;
    clearInput();inventory_->clear();inventory_->add(Item::OakBoat);
    player_=std::make_unique<Player>(glm::vec3(-12,78.5f,-12));player_->setEquipment(inventory_.get());player_->addMouseMovement(0,-330);
    input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(transportation_->boats().size()==1&&inventory_->selectedStack().empty(),"RMB boat placement consumes one owned item");
    clearInput();input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;interaction_.mobTarget={0,1,{0,0,0}};handleUseAction();check(!transportation_->mounted(),"nearer mob retains right-click priority");
    interaction_.mobTarget={};clearInput();input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(transportation_->mounted()&&player_->isRiding(),"RMB boat mount handler");
    const auto before=player_->position();clearInput();input_.keys_[GLFW_KEY_W]=true;
    for(int i=0;i<120;++i)updateTransportation(1.f/60,true);
    check(player_->position().z<before.z-5,"bound movement input drives boat");clearInput();input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;updateTransportation(.02f,true);
    check(!transportation_->mounted()&&!player_->isRiding(),"bound Sneak dismount handler");
    player_->addMouseMovement(1800,0);clearInput();input_.mouseButtons_[GLFW_MOUSE_BUTTON_LEFT]=true;
    const auto drops=survival_->renderItemSprites().size();interaction_.attackCooldown=0;updatePlayingInteraction(.02f);
    check(transportation_->boats().empty()&&survival_->renderItemSprites().size()==drops+1,"LMB one boat recovery handler");
    if(Persistence::enabled()){
        seed_=991;inventory_->clear();const auto first=transportation_->createMap(player_->position());inventory_->add(Item::FilledMap,1,0,first);
        check(saveAll(),"game checkpoint initial save");
#ifdef _WIN32
        const auto inventoryPath=worldSavePath("voxel_inventory.vxi");
        HANDLE locked=CreateFileW(std::filesystem::path(inventoryPath).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);check(locked!=INVALID_HANDLE_VALUE,"game inventory lock");
        inventory_->clear();const auto second=transportation_->createMap(player_->position());inventory_->add(Item::FilledMap,1,0,second);
        const bool saved=saveAll();const bool retryScheduled=saveRetryAt_>glfwGetTime()&&saveRetryCount_==1;
        returnToMainMenu();const bool retained=world_!=nullptr;CloseHandle(locked);
        check(!saved&&retryScheduled&&retained,"game failure schedules nonblocking recovery and refuses world teardown");
        Inventory old;Transportation previous({0,0,0});check(old.load(inventoryPath,991)&&previous.load(worldSavePath("voxel_travel.vtn"),991)&&old.slot(0).identity==first&&previous.map(first)&&!previous.map(second),"failed game save keeps coherent map/inventory checkpoint");
        check(saveAll()&&saveRetryAt_==0&&saveRetryCount_==0,"game retry commits and clears schedule");
        check(old.load(inventoryPath,991)&&previous.load(worldSavePath("voxel_travel.vtn"),991)&&old.slot(0).identity==second&&previous.map(second),"successful retry map/inventory consistency");
        HANDLE settingsLock=CreateFileW(std::filesystem::path("voxel_settings.cfg").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);check(settingsLock!=INVALID_HANDLE_VALUE,"independent settings lock");
        inventory_->add(Item::Diamond);const bool allSaved=saveAll();CloseHandle(settingsLock);
        check(!allSaved&&old.load(inventoryPath,991)&&old.count(Item::Diamond)==1,"world checkpoint remains coherent when independent settings fail");
        check(saveAll(),"settings retry succeeds without rolling back world");
#endif
    }
    clearInput();std::cout<<report<<"; native input paths: waypoints, map/offhand/paid zoom, RMB boat placement/mount, bound movement/Sneak and LMB recovery passed"<<std::endl;
}
