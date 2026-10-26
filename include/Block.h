#pragma once
#include <cstdint>

// A block value is a state: low 16 bits are the stable type ID, high 16 bits properties.
enum class Block : std::uint32_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Log,
    Leaves,
    Water,
    CoalOre,
    IronOre,
    GoldOre,
    CopperOre,
    DiamondOre,
    Planks,
    CraftingTable,
    Torch,
    Farmland,
    Crop0,
    Crop1,
    Crop2,
    Crop3,
    WaterFlow1,
    WaterFlow2,
    WaterFlow3,
    WaterFlow4,
    WaterFlow5,
    WaterFlow6,
    WaterFlow7,
    WaterFalling,
    Cobblestone,
    StoneBricks,
    Bricks,
    Glass,
    Gravel,
    Clay,
    Snow,
    SnowBlock,
    BirchPlanks,
    BirchLog,
    BirchLeaves,
    Cactus,
    Furnace,
    Bookshelf,
    WoodenDoor,
    WoodenSlab,
    StoneSlab,
    Granite,
    Diorite,
    Andesite,
    MossyCobblestone,
    MossyStoneBricks,
    Ice,
    Mud,
    TallGrass,
    RedFlower,
    YellowFlower,
    LadderNorth,
    LadderSouth,
    LadderEast,
    LadderWest,
    Chest,
    WoodenSlabTop,
    StoneSlabTop,
    DoorClosedNorthLower,
    DoorClosedNorthUpper,
    DoorClosedSouthLower,
    DoorClosedSouthUpper,
    DoorClosedEastLower,
    DoorClosedEastUpper,
    DoorClosedWestLower,
    DoorClosedWestUpper,
    DoorOpenNorthLower,
    DoorOpenNorthUpper,
    DoorOpenSouthLower,
    DoorOpenSouthUpper,
    DoorOpenEastLower,
    DoorOpenEastUpper,
    DoorOpenWestLower,
    DoorOpenWestUpper,
    DoorRightClosedNorthLower,
    DoorRightClosedNorthUpper,
    DoorRightClosedSouthLower,
    DoorRightClosedSouthUpper,
    DoorRightClosedEastLower,
    DoorRightClosedEastUpper,
    DoorRightClosedWestLower,
    DoorRightClosedWestUpper,
    DoorRightOpenNorthLower,
    DoorRightOpenNorthUpper,
    DoorRightOpenSouthLower,
    DoorRightOpenSouthUpper,
    DoorRightOpenEastLower,
    DoorRightOpenEastUpper,
    DoorRightOpenWestLower,
    DoorRightOpenWestUpper,
    CoalBlock,
    IronBlock,
    GoldBlock,
    CopperBlock,
    DiamondBlock,
    HayBale,
    Sandstone,
    PolishedGranite,
    PolishedDiorite,
    PolishedAndesite,
    SugarCane,
    VineNorth,
    VineSouth,
    VineEast,
    VineWest,
    LightningRod,
    WetFarmland,
    Fire,
    Snow2, Snow3, Snow4, Snow5, Snow6, Snow7, Snow8,
    // Appended IDs: existing worlds and inventory save IDs remain stable.
    SpruceLog, SprucePlanks, SpruceLeaves,
    JungleLog, JunglePlanks, JungleLeaves,
    AcaciaLog, AcaciaPlanks, AcaciaLeaves,
    DarkOakLog, DarkOakPlanks, DarkOakLeaves,
    Podzol, Mycelium, CoarseDirt,
    PackedIce, BlueIce, Terracotta,
    TerracottaRed, TerracottaOrange, TerracottaYellow,
    TerracottaWhite, TerracottaBrown, Bamboo,
    Fern, DeadBush, LilyPad,
    RedMushroom, BrownMushroom, MushroomStem,
    RedMushroomBlock, BrownMushroomBlock,
    // Generator V11: append only; historical save IDs are unchanged.
    Deepslate, DeepslateCoalOre, DeepslateIronOre, DeepslateCopperOre, DeepslateGoldOre, DeepslateDiamondOre, Bedrock, Lava,
    Cobweb, CrackedStoneBricks,
    LavaFlow1, LavaFlow2, LavaFlow3, LavaFlow4, LavaFalling,
    // v2.15.0 crop stages and persistent composter fill.
    Carrot0, Carrot1, Carrot2, Carrot3, Potato0, Potato1, Potato2, Potato3, Beetroot0, Beetroot1, Beetroot2, Beetroot3, PumpkinStem0, PumpkinStem1, PumpkinStem2, PumpkinStem3, MelonStem0, MelonStem1, MelonStem2, MelonStem3, Pumpkin, Melon, BerryBush0, BerryBush1, BerryBush2, BerryBush3, Composter0, Composter1, Composter2, Composter3, Composter4,
    // v2.16.0: stable types; orientation and connections are properties.
    OakStairs, OakSlab, OakFence, OakFenceGate, OakTrapdoor, OakDoor, OakStrippedLog, OakButton, OakPressurePlate, OakTable, OakChair, OakBench, OakSign, OakShelf, BirchStairs, BirchSlab, BirchFence, BirchFenceGate, BirchTrapdoor, BirchDoor, BirchStrippedLog, BirchButton, BirchPressurePlate, BirchTable, BirchChair, BirchBench, BirchSign, BirchShelf, SpruceStairs, SpruceSlab, SpruceFence, SpruceFenceGate, SpruceTrapdoor, SpruceDoor, SpruceStrippedLog, SpruceButton, SprucePressurePlate, SpruceTable, SpruceChair, SpruceBench, SpruceSign, SpruceShelf, JungleStairs, JungleSlab, JungleFence, JungleFenceGate, JungleTrapdoor, JungleDoor, JungleStrippedLog, JungleButton, JunglePressurePlate, JungleTable, JungleChair, JungleBench, JungleSign, JungleShelf, AcaciaStairs, AcaciaSlab, AcaciaFence, AcaciaFenceGate, AcaciaTrapdoor, AcaciaDoor, AcaciaStrippedLog, AcaciaButton, AcaciaPressurePlate, AcaciaTable, AcaciaChair, AcaciaBench, AcaciaSign, AcaciaShelf, DarkOakStairs, DarkOakSlab, DarkOakFence, DarkOakFenceGate, DarkOakTrapdoor, DarkOakDoor, DarkOakStrippedLog, DarkOakButton, DarkOakPressurePlate, DarkOakTable, DarkOakChair, DarkOakBench, DarkOakSign, DarkOakShelf, CobblestoneStairs, CobblestoneSlab, CobblestoneWall, StoneStairs, StoneCutSlab, StoneWall, StoneBricksStairs, StoneBricksSlab, StoneBricksWall, MossyCobblestoneStairs, MossyCobblestoneSlab, MossyCobblestoneWall, MossyStoneBricksStairs, MossyStoneBricksSlab, MossyStoneBricksWall, BricksStairs, BricksSlab, BricksWall, SandstoneStairs, SandstoneSlab, SandstoneWall, DeepslateStairs, DeepslateSlab, DeepslateWall, PolishedGraniteStairs, PolishedGraniteSlab, PolishedGraniteWall, PolishedDioriteStairs, PolishedDioriteSlab, PolishedDioriteWall, PolishedAndesiteStairs, PolishedAndesiteSlab, PolishedAndesiteWall, ChiseledStoneBricks, SmoothStone, SmoothSandstone, CutSandstone, PolishedDeepslate, DeepslateBricks, CrackedDeepslateBricks, RoofShingles, GlassPane, RoseGlassPane, AmberGlassPane, SmokedGlassPane, Lantern, Chain, Candle, Campfire, Bed, Barrel, Painting, ItemFrame, FlowerPot, Carpet, DirtPath, StonePaving, Scaffold,
    // v2.17.0: append only; all signal and facing values are state properties.
    PowerWire, Lever, Repeater, AndGate, OrGate, NotGate, PowerLamp, Hopper, Piston, StickyPiston, DaylightSensor, Observer, CropSensor, AutoHarvester, PistonHead,
    Count
};

using BlockId = std::uint16_t;
inline Block blockType(Block b) { return Block(std::uint32_t(b)&0xffffU); }
inline std::uint16_t blockProperties(Block b) { return std::uint16_t(std::uint32_t(b)>>16U); }
inline Block blockState(Block type, std::uint16_t properties) { return Block(std::uint32_t(blockType(type)) | (std::uint32_t(properties)<<16U)); }
bool validBlockState(Block block);
inline bool isBuilding(Block b){const auto t=blockType(b);return t>=Block::OakStairs && t<Block::Count;}
bool buildingSolid(Block block);
bool buildingTransparent(Block block);
bool buildingOccludesLight(Block block);
std::uint8_t buildingEmission(Block block);

inline bool isLava(Block b) { return b == Block::Lava || (b>=Block::LavaFlow1 && b<=Block::LavaFalling); }
inline bool isLavaSource(Block b) { return b==Block::Lava; }
inline int lavaLevel(Block b) {
    if(b==Block::Lava || b==Block::LavaFalling) return 0;
    return isLava(b)?int(b)-int(Block::LavaFlow1)+1:5;
}
inline Block flowingLavaBlock(int level) {return level<1?Block::Lava:level>4?Block::Air:Block(int(Block::LavaFlow1)+level-1);}

inline float lavaHeight(Block b) {return b==Block::Lava || b==Block::LavaFalling?1.f:isLava(b)?1.f-lavaLevel(b)*.125f:0.f;}

inline bool isWater(Block b) {
    return b == Block::Water ||
           (b >= Block::WaterFlow1 && b <= Block::WaterFalling);
}
inline bool isWaterSource(Block b) {
    return b == Block::Water;
}
inline int waterLevel(Block b) {
    if (b == Block::Water || b == Block::WaterFalling)
        return 0;
    if (b >= Block::WaterFlow1 && b <= Block::WaterFlow7)
        return static_cast<int>(b) - static_cast<int>(Block::WaterFlow1) + 1;
    return 8;
}
inline Block flowingWaterBlock(int level) {
    if (level <= 0)
        return Block::Water;
    if (level >= 8)
        return Block::Air;
    return static_cast<Block>(static_cast<int>(Block::WaterFlow1) + level - 1);
}
inline float waterHeight(Block b) {
    if (!isWater(b))
        return 0.0f;
    if (b == Block::Water || b == Block::WaterFalling)
        return 0.875f;
    return 0.875f - static_cast<float>(waterLevel(b)) * 0.09375f;
}
inline bool isTorch(Block b) {
    return blockType(b) == Block::Torch;
}

// The existing block-light solver's source levels. Keep indexing and solving
// on the same definition; non-emissive blocks must not enter the source index.
inline std::uint8_t blockLightEmission(Block block) {
    return isBuilding(block)?buildingEmission(block):isTorch(block) || block == Block::Fire || isLava(block) ? 15U : 0U;
}
inline bool isCrop(Block b) {
    return (b >= Block::Crop0 && b <= Block::Crop3) ||
           (b >= Block::Carrot0 && b <= Block::MelonStem3);
}
inline bool isBerryBush(Block b) { return b >= Block::BerryBush0 && b <= Block::BerryBush3; }
inline bool isComposter(Block b) { return b >= Block::Composter0 && b <= Block::Composter4; }
inline bool isFarmland(Block b) { return b == Block::Farmland || b == Block::WetFarmland; }
inline int cropStage(Block b) { return b <= Block::Crop3 ? int(b)-int(Block::Crop0) : (int(b)-int(Block::Carrot0))%4; }
inline Block cropBase(Block b) { return Block(int(b)-cropStage(b)); }
inline bool isFarmTracked(Block b) { return isCrop(b) || isBerryBush(b) || isFarmland(b) || b==Block::SugarCane || b==Block::RedMushroom || b==Block::BrownMushroom; }
static_assert(int(Block::Count) <= 65535, "Block type ID overflow");
inline bool isLog(Block b) {
    b=blockType(b);
    return b==Block::Log||b==Block::BirchLog||b==Block::SpruceLog||b==Block::JungleLog||
           b==Block::AcaciaLog||b==Block::DarkOakLog;
}
inline bool isLeaf(Block b) {
    return b == Block::Leaves || b == Block::BirchLeaves ||
           b == Block::SpruceLeaves || b == Block::JungleLeaves ||
           b == Block::AcaciaLeaves || b == Block::DarkOakLeaves;
}
inline bool isPlant(Block b) {
    return isCrop(b) || isBerryBush(b) || b == Block::TallGrass || b == Block::RedFlower || b == Block::YellowFlower ||
           b == Block::SugarCane || b == Block::Fire || b == Block::Bamboo ||
           b == Block::Cobweb || b == Block::Fern || b == Block::DeadBush || b == Block::RedMushroom || b == Block::BrownMushroom;
}

inline bool isVine(Block b) {
    return b >= Block::VineNorth && b <= Block::VineWest;
}

inline bool isLadder(Block b) {
    return b >= Block::LadderNorth && b <= Block::LadderWest;
}
inline bool isWallAttachment(Block b) { return isLadder(b) || isVine(b); }
inline bool isTransparentBlock(Block b) {
    if(isBuilding(b))return buildingTransparent(b);
    return isWater(b) || b == Block::Glass || b == Block::Ice || isLeaf(b) || isPlant(b) ||
           isWallAttachment(b);
}
inline bool isSlab(Block b) {
    return b == Block::WoodenSlab || b == Block::StoneSlab || b == Block::WoodenSlabTop ||
           b == Block::StoneSlabTop;
}
inline bool isTopSlab(Block b) {
    return b == Block::WoodenSlabTop || b == Block::StoneSlabTop;
}
inline bool isWoodenSlab(Block b) {
    return b == Block::WoodenSlab || b == Block::WoodenSlabTop;
}
inline bool isStoneSlab(Block b) {
    return b == Block::StoneSlab || b == Block::StoneSlabTop;
}
inline bool isDoor(Block b) {
    return b == Block::WoodenDoor ||
           (b >= Block::DoorClosedNorthLower && b <= Block::DoorRightOpenWestUpper);
}
inline bool isDoorUpper(Block b) {
    if (b == Block::WoodenDoor)
        return false;
    return isDoor(b) && ((static_cast<int>(b) - static_cast<int>(Block::DoorClosedNorthLower)) % 2) == 1;
}
inline bool isDoorOpen(Block b) {
    return (b >= Block::DoorOpenNorthLower && b <= Block::DoorOpenWestUpper) ||
           (b >= Block::DoorRightOpenNorthLower && b <= Block::DoorRightOpenWestUpper);
}
inline bool doorHingeRight(Block b) {
    return b >= Block::DoorRightClosedNorthLower && b <= Block::DoorRightOpenWestUpper;
}
inline int doorFacing(Block b) {
    if (!isDoor(b) || b == Block::WoodenDoor)
        return 0;
    const int value = static_cast<int>(b);
    const int base = doorHingeRight(b)
                         ? static_cast<int>(isDoorOpen(b) ? Block::DoorRightOpenNorthLower
                                                           : Block::DoorRightClosedNorthLower)
                         : static_cast<int>(isDoorOpen(b) ? Block::DoorOpenNorthLower
                                                           : Block::DoorClosedNorthLower);
    return (value - base) / 2;
}
inline Block doorBlock(int facing, bool upper, bool hingeRight, bool open) {
    const int base = static_cast<int>(hingeRight
        ? (open ? Block::DoorRightOpenNorthLower : Block::DoorRightClosedNorthLower)
        : (open ? Block::DoorOpenNorthLower : Block::DoorClosedNorthLower));
    return static_cast<Block>(base + (facing & 3) * 2 + (upper ? 1 : 0));
}
inline Block doorBlock(int facing, bool upper, bool open) {
    return doorBlock(facing, upper, false, open);
}
inline int snowLayers(Block b) {
    return b==Block::Snow ? 1 : b>=Block::Snow2 && b<=Block::Snow8 ?
        static_cast<int>(b)-static_cast<int>(Block::Snow2)+2 : 0;
}
inline Block snowLayerBlock(int layers) {
    return layers<=0 ? Block::Air : layers==1 ? Block::Snow :
        static_cast<Block>(static_cast<int>(Block::Snow2)+layers-2);
}
inline float blockCollisionMinY(Block b) {
    return isTopSlab(b) ? 0.5f : 0.0f;
}
inline float blockCollisionMaxY(Block b) {
    if (snowLayers(b))
        return snowLayers(b)*0.125f;
    return isSlab(b) ? (isTopSlab(b) ? 1.0f : 0.5f) : 1.0f;
}
inline float blockCollisionHeight(Block b) {
    if (isSlab(b))
        return blockCollisionMaxY(b) - blockCollisionMinY(b);
    if (snowLayers(b))
        return snowLayers(b)*0.125f;
    return 1.0f;
}
inline bool isSolid(Block b) {
    if(isBuilding(b))return buildingSolid(b);
    return b != Block::Air && !isWater(b) && !isLava(b) && !isTorch(b) && !isCrop(b) &&
           b != Block::Snow && b != Block::LilyPad && b != Block::LightningRod && b != Block::Fire && !isPlant(b) && !isWallAttachment(b);
}
inline bool isRenderable(Block b) {
    return b != Block::Air;
}
inline bool occludesLight(Block b) {
    if(isBuilding(b))return buildingOccludesLight(b);
    return isSolid(b) && !isLeaf(b) && b != Block::Glass && b != Block::Ice && !isSlab(b) &&
           !isDoor(b) && !snowLayers(b);
}

enum class BlockShape : std::uint8_t {
    Empty,
    Cube,
    PartialCube,
    Fluid,
    Thin,
    Crossed
};

struct BlockGeometryProperties {
    BlockShape shape = BlockShape::Cube;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 1.0f;
    float maxY = 1.0f;
    float maxZ = 1.0f;
    bool occludesNeighborFaces = true;
    bool mergeMatchingFaces = false;
    // Fraction of full-cube contact occlusion. Independent of light emission.
    float aoOcclusion = 1.0f;
};

BlockGeometryProperties buildingGeometry(Block block);

// Describes the actual occupied bounds used by face occlusion. Rendering may use a
// custom mesh (doors, ladders and crossed plants), but neighboring cube faces are
// culled only when these bounds really cover their shared boundary.
inline BlockGeometryProperties blockGeometry(Block block) {
    if(isBuilding(block))return buildingGeometry(block);
    BlockGeometryProperties geometry;
    if (block == Block::Air) {
        geometry.shape = BlockShape::Empty;
        geometry.maxX = geometry.maxY = geometry.maxZ = 0.0f;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (block == Block::LilyPad) {
        geometry.shape = BlockShape::PartialCube;
        geometry.maxY = .0625f;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0;
    } else if (isWater(block) || isLava(block)) {
        geometry.shape = BlockShape::Fluid;
        geometry.maxY = isLava(block) ? lavaHeight(block) : waterHeight(block);
        geometry.occludesNeighborFaces = false;
        geometry.mergeMatchingFaces = true;
        geometry.aoOcclusion = 0.0f;
    } else if (block == Block::LightningRod) {
        geometry.shape = BlockShape::Thin;
        geometry.minX = geometry.minZ = .4375f;
        geometry.maxX = geometry.maxZ = .5625f;
        geometry.maxY = 1.0f;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0;
    } else if (block == Block::Fire) {
        geometry.shape = BlockShape::Crossed;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0;
    } else if (snowLayers(block)) {
        geometry.shape = BlockShape::PartialCube;
        geometry.maxY = snowLayers(block)*0.125f;
        geometry.aoOcclusion = 0.25f;
    } else if (isSlab(block)) {
        geometry.shape = BlockShape::PartialCube;
        geometry.minY = isTopSlab(block) ? 0.5f : 0.0f;
        geometry.maxY = isTopSlab(block) ? 1.0f : 0.5f;
        geometry.aoOcclusion = 0.85f;
    } else if (isDoor(block)) {
        constexpr float thickness = 0.1875f;
        geometry.shape = BlockShape::Thin;
        geometry.occludesNeighborFaces = false;
        geometry.mergeMatchingFaces = true;
        geometry.aoOcclusion = 0.65f;
        const int facing = doorFacing(block);
        const bool right = doorHingeRight(block);
        if (!isDoorOpen(block)) {
            if (facing == 0) geometry.maxZ = thickness;
            else if (facing == 1) geometry.minZ = 1.0f - thickness;
            else if (facing == 2) geometry.minX = 1.0f - thickness;
            else geometry.maxX = thickness;
        } else {
            if (facing == 0 || facing == 1) {
                const bool hingeAtMaximum = (facing == 0) ? right : !right;
                if (hingeAtMaximum) geometry.minX = 1.0f - thickness;
                else geometry.maxX = thickness;
            } else {
                const bool hingeAtMaximum = (facing == 2) ? right : !right;
                if (hingeAtMaximum) geometry.minZ = 1.0f - thickness;
                else geometry.maxZ = thickness;
            }
        }
    } else if (isWallAttachment(block)) {
        if(block==Block::LadderNorth||block==Block::VineNorth)geometry.maxZ=.07f;
        else if(block==Block::LadderSouth||block==Block::VineSouth)geometry.minZ=.93f;
        else if(block==Block::LadderEast||block==Block::VineEast)geometry.minX=.93f;
        else geometry.maxX=.07f;
        geometry.shape = BlockShape::Thin;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (isPlant(block) || isCrop(block)) {
        geometry.shape = BlockShape::Crossed;
        if (isCrop(block) || isBerryBush(block)) {
            geometry.minX = geometry.minZ = .12f;
            geometry.maxX = geometry.maxZ = .88f;
            geometry.maxY = isCrop(block) ? .25f + .20f * cropStage(block) : .35f + .18f*(int(block)-int(Block::BerryBush0));
        }
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (isTorch(block)) {
        geometry.minX = geometry.minZ = 0.4375f;
        geometry.maxX = geometry.maxZ = 0.5625f;
        geometry.maxY = 0.75f;
        if(blockProperties(block)) {geometry.minY=.25f;geometry.maxY=.9f;switch((blockProperties(block)-1)&3){case 0:geometry.minZ=.875f;geometry.maxZ=1;break;case 1:geometry.minX=0;geometry.maxX=.125f;break;case 2:geometry.minZ=0;geometry.maxZ=.125f;break;case 3:geometry.minX=.875f;geometry.maxX=1;break;}}
        geometry.shape = BlockShape::Thin;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (block == Block::Glass || block == Block::Ice || isLeaf(block)) {
        geometry.occludesNeighborFaces = false;
        geometry.mergeMatchingFaces = true;
        geometry.aoOcclusion = isLeaf(block) ? 0.35f :
                               block == Block::Ice ? 0.06f : 0.0f;
    } else if (block == Block::Cactus) {
        geometry.aoOcclusion = 0.45f;
    } else if (block == Block::Farmland || block == Block::WetFarmland) {
        geometry.aoOcclusion = 0.85f;
    }
    return geometry;
}

inline bool matchingOcclusionGroup(Block first, Block second) {
    if((first==Block::Glass||isBuilding(first)) && (second==Block::Glass||isBuilding(second)) && (first==Block::Glass||buildingTransparent(first)) && (second==Block::Glass||buildingTransparent(second)))return true;
    if (first == second)
        return blockGeometry(first).mergeMatchingFaces;
    return isDoor(first) && isDoor(second) && doorFacing(first) == doorFacing(second) &&
           isDoorOpen(first) == isDoorOpen(second) &&
           doorHingeRight(first) == doorHingeRight(second);
}
