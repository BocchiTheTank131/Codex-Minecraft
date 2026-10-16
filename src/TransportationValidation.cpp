#include "Game.h"
#include "Transportation.h"
#include "Player.h"
#include "World.h"
#include "Persistence.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

bool Transportation::selfTest(World& w,std::string& report){
    auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    // Negative-coordinate basin with an east wall, dry platform, head clearance and a chunk boundary.
    for(int x=-32;x<=16;x+=16)for(int z=-32;z<=16;z+=16)w.prepareSpawnTerrain({float(x),80,float(z)});
    w.fillBlocks({-30,73,-30},{12,88,12},Block::Air);
    w.fillBlocks({-30,74,-30},{12,74,12},Block::Stone);
    w.fillBlocks({-30,75,-30},{12,77,12},Block::Water);
    w.fillBlocks({5,75,-30},{6,82,12},Block::Stone);
    SurvivalWorld mobs(991);Inventory inventory;
    for(int fps:{15,30,60,240}){
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
        check(t.boats_[0].position.x<4.34f,"spike tunneling");
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

    const auto id=t.createMap({-10,80,-10});const auto id2=t.createMap({-10,80,-10});check(id&&id2!=id,"map identity");
    for(auto pixel:t.map(id)->pixels)check(pixel==0,"map starts hidden");
    Player explorer({-12,79,-12});t.reveal(w,explorer,id);t.finishMapJob(true);
    check(std::count_if(t.map(id)->pixels.begin(),t.map(id)->pixels.end(),[](auto c){return c!=0;})>0,"loaded map survey");
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
        auto* container=w.chestAt({-10,79,-10});(void)container;
        w.setBlock(-10,79,-10,Block::Chest);w.chestAt({-10,79,-10},true)->slots[0]=dropped;
        check(w.saveWorld("transport-tests/world.vxw",explorer.position()),"chest world save");World copy(w.seed(),22);glm::vec3 spawn;check(copy.loadWorld("transport-tests/world.vxw",spawn),"chest reload");check(copy.chestAt({-10,79,-10})->slots[0].identity==id,"chest map identity reload");
    }else check(t.save("transport-demo-forbidden.vtn",991)&&!std::filesystem::exists("transport-demo-forbidden.vtn"),"demo stateless navigation");
    report="Transportation passed: six woods, 15/30/60/240 FPS, 500ms spike, shores, negative chunk boundary, buoyancy, mount/dismount, independent maps, fog/zoom, chest/drop identities, waypoints, versioned persistence/demo guards";return true;
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
    clearInput();input_.keys_[GLFW_KEY_ESCAPE]=true;navigationInput();check(!navigationOpen_,"navigation Escape");
    clearInput();inventory_->clear();inventory_->add(Item::EmptyMap);interaction_.hasBlockTarget=false;input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();
    check(inventory_->selectedStack().item==Item::FilledMap&&inventory_->selectedStack().identity&&mapView_,"RMB map creation owns one map");const auto id=inventory_->selectedStack().identity;
    input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(mapView_&&inventory_->selectedStack().identity==id,"held RMB does not repeat use");
    inventory_->swapOffhand();clearInput();input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();check(inventory_->equipment(4).identity==id&&!mapView_,"offhand map use");
    inventory_->add(Item::Paper,8);clearInput();input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;handleUseAction();
    check(inventory_->equipment(4).identity!=id&&transportation_->map(inventory_->equipment(4).identity)->scale==1&&inventory_->count(Item::Paper)==0,"zoom input consumes eight paper once");
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
    clearInput();std::cout<<report<<"; native input paths: waypoints, map/offhand/paid zoom, RMB boat placement/mount, bound movement/Sneak and LMB recovery passed"<<std::endl;
}
