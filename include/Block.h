#pragma once
#include <cstdint>

enum class Block : std::uint8_t {
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
    Count
};

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
    return b == Block::Torch;
}

// The existing block-light solver's source levels. Keep indexing and solving
// on the same definition; non-emissive blocks must not enter the source index.
inline std::uint8_t blockLightEmission(Block block) {
    return block == Block::Torch ? 15U : 0U;
}
inline bool isCrop(Block b) {
    return b >= Block::Crop0 && b <= Block::Crop3;
}
inline bool isLeaf(Block b) {
    return b == Block::Leaves || b == Block::BirchLeaves;
}
inline bool isPlant(Block b) {
    return b == Block::TallGrass || b == Block::RedFlower || b == Block::YellowFlower ||
           b == Block::SugarCane;
}

inline bool isVine(Block b) {
    return b >= Block::VineNorth && b <= Block::VineWest;
}

inline bool isLadder(Block b) {
    return b >= Block::LadderNorth && b <= Block::LadderWest;
}
inline bool isWallAttachment(Block b) { return isLadder(b) || isVine(b); }
inline bool isTransparentBlock(Block b) {
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
inline float blockCollisionMinY(Block b) {
    return isTopSlab(b) ? 0.5f : 0.0f;
}
inline float blockCollisionMaxY(Block b) {
    if (b == Block::Snow)
        return 0.125f;
    return isSlab(b) ? (isTopSlab(b) ? 1.0f : 0.5f) : 1.0f;
}
inline float blockCollisionHeight(Block b) {
    if (isSlab(b))
        return blockCollisionMaxY(b) - blockCollisionMinY(b);
    if (b == Block::Snow)
        return 0.125f;
    return 1.0f;
}
inline bool isSolid(Block b) {
    return b != Block::Air && !isWater(b) && b != Block::Torch && !isCrop(b) &&
           b != Block::Snow && !isPlant(b) && !isWallAttachment(b);
}
inline bool isRenderable(Block b) {
    return b != Block::Air;
}
inline bool occludesLight(Block b) {
    return isSolid(b) && !isLeaf(b) && b != Block::Glass && b != Block::Ice && !isSlab(b) &&
           !isDoor(b);
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

// Describes the actual occupied bounds used by face occlusion. Rendering may use a
// custom mesh (doors, ladders and crossed plants), but neighboring cube faces are
// culled only when these bounds really cover their shared boundary.
inline BlockGeometryProperties blockGeometry(Block block) {
    BlockGeometryProperties geometry;
    if (block == Block::Air) {
        geometry.shape = BlockShape::Empty;
        geometry.maxX = geometry.maxY = geometry.maxZ = 0.0f;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (isWater(block)) {
        geometry.shape = BlockShape::Fluid;
        geometry.maxY = waterHeight(block);
        geometry.occludesNeighborFaces = false;
        geometry.mergeMatchingFaces = true;
        geometry.aoOcclusion = 0.0f;
    } else if (block == Block::Snow) {
        geometry.shape = BlockShape::PartialCube;
        geometry.maxY = 0.125f;
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
        geometry.shape = BlockShape::Thin;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (isPlant(block) || isCrop(block)) {
        geometry.shape = BlockShape::Crossed;
        geometry.occludesNeighborFaces = false;
        geometry.aoOcclusion = 0.0f;
    } else if (block == Block::Torch) {
        geometry.minX = geometry.minZ = 0.4375f;
        geometry.maxX = geometry.maxZ = 0.5625f;
        geometry.maxY = 0.75f;
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
    } else if (block == Block::Farmland) {
        geometry.aoOcclusion = 0.85f;
    }
    return geometry;
}

inline bool matchingOcclusionGroup(Block first, Block second) {
    if (first == second)
        return blockGeometry(first).mergeMatchingFaces;
    return isDoor(first) && isDoor(second) && doorFacing(first) == doorFacing(second) &&
           isDoorOpen(first) == isDoorOpen(second) &&
           doorHingeRight(first) == doorHingeRight(second);
}
