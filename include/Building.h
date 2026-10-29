#pragma once
#include "Definitions.h"
#include <array>
#include <optional>
#include <glm/glm.hpp>
class World;
class Game;
enum class BuildingShape {Cube, Stairs, Slab, Fence, FenceGate, Trapdoor, Door, StrippedLog, Button, PressurePlate, Table, Chair, Bench, Sign, Shelf, Wall, Pane, Lantern, Chain, Candle, Campfire, Bed, Barrel, Painting, Frame, Pot, Carpet, Path, Scaffold, Wire, Lever, Circuit, Hopper, Piston, Machine, PistonHead};
struct BuildingDefinition {Block block;Item item;BuildingShape shape;Block material;Item ingredient;const char* name;};
const std::array<BuildingDefinition,142>& buildingContent();
const std::array<BuildingDefinition,15>& automationContent();
const BuildingDefinition* buildingDefinition(Block);
const BuildingDefinition* buildingDefinition(Item);
bool isBuilding(Block);
bool buildingSolid(Block);
bool buildingTransparent(Block);
bool validBuildingState(Block);
std::uint8_t buildingEmission(Block);
struct BuildingBox {glm::vec3 minimum{0},maximum{1};};
struct BuildingBoxes {std::array<BuildingBox,24> boxes{};int count=0;void add(float x,float y,float z,float X,float Y,float Z){boxes[count++]={{x,y,z},{X,Y,Z}};} };
BuildingBoxes buildingBoxes(Block,bool collision=false);
BlockGeometryProperties buildingGeometry(Block);
Block buildingConnectedState(const World&,const glm::ivec3&,Block);
Block buildingPlacement(Block,const glm::vec3& look,const glm::ivec3& normal,const glm::vec3& hit);
int buildingTexture(Block,int face);
bool buildingNeedsSupport(Block);
std::optional<glm::ivec3> blockSupportPosition(glm::ivec3,Block);
bool buildingIsContainer(Block);
bool buildingIsBed(Block);
bool buildingIsDoor(Block);
glm::ivec3 buildingFacing(int);
