#pragma once
#include "Definitions.h"
#include <array>
enum class BiomeMaterial { Wood, Foliage, Ground, Ice, Plant, Stone };
struct BiomeContentEntry { Block block; Item item; const char* name; int side, top, bottom; BiomeMaterial material; };
inline const std::array<BiomeContentEntry,32>& biomeContent() {
    static const std::array<BiomeContentEntry,32> entries{{
        {Block::SpruceLog,Item::SpruceLog,"SpruceLog",60,61,61,BiomeMaterial::Wood},
        {Block::SprucePlanks,Item::SprucePlanks,"SprucePlanks",62,62,62,BiomeMaterial::Wood},
        {Block::SpruceLeaves,Item::SpruceLeaves,"SpruceLeaves",63,63,63,BiomeMaterial::Foliage},
        {Block::JungleLog,Item::JungleLog,"JungleLog",64,65,65,BiomeMaterial::Wood},
        {Block::JunglePlanks,Item::JunglePlanks,"JunglePlanks",66,66,66,BiomeMaterial::Wood},
        {Block::JungleLeaves,Item::JungleLeaves,"JungleLeaves",67,67,67,BiomeMaterial::Foliage},
        {Block::AcaciaLog,Item::AcaciaLog,"AcaciaLog",68,69,69,BiomeMaterial::Wood},
        {Block::AcaciaPlanks,Item::AcaciaPlanks,"AcaciaPlanks",70,70,70,BiomeMaterial::Wood},
        {Block::AcaciaLeaves,Item::AcaciaLeaves,"AcaciaLeaves",71,71,71,BiomeMaterial::Foliage},
        {Block::DarkOakLog,Item::DarkOakLog,"DarkOakLog",72,73,73,BiomeMaterial::Wood},
        {Block::DarkOakPlanks,Item::DarkOakPlanks,"DarkOakPlanks",74,74,74,BiomeMaterial::Wood},
        {Block::DarkOakLeaves,Item::DarkOakLeaves,"DarkOakLeaves",75,75,75,BiomeMaterial::Foliage},
        {Block::Podzol,Item::Podzol,"Podzol",76,76,2,BiomeMaterial::Ground},
        {Block::Mycelium,Item::Mycelium,"Mycelium",77,77,2,BiomeMaterial::Ground},
        {Block::CoarseDirt,Item::CoarseDirt,"CoarseDirt",78,78,78,BiomeMaterial::Ground},
        {Block::PackedIce,Item::PackedIce,"PackedIce",79,79,79,BiomeMaterial::Ice},
        {Block::BlueIce,Item::BlueIce,"BlueIce",80,80,80,BiomeMaterial::Ice},
        {Block::Terracotta,Item::Terracotta,"Terracotta",81,81,81,BiomeMaterial::Stone},
        {Block::TerracottaRed,Item::TerracottaRed,"TerracottaRed",82,82,82,BiomeMaterial::Stone},
        {Block::TerracottaOrange,Item::TerracottaOrange,"TerracottaOrange",83,83,83,BiomeMaterial::Stone},
        {Block::TerracottaYellow,Item::TerracottaYellow,"TerracottaYellow",84,84,84,BiomeMaterial::Stone},
        {Block::TerracottaWhite,Item::TerracottaWhite,"TerracottaWhite",85,85,85,BiomeMaterial::Stone},
        {Block::TerracottaBrown,Item::TerracottaBrown,"TerracottaBrown",86,86,86,BiomeMaterial::Stone},
        {Block::Bamboo,Item::Bamboo,"Bamboo",87,87,87,BiomeMaterial::Plant},
        {Block::Fern,Item::Fern,"Fern",88,88,88,BiomeMaterial::Plant},
        {Block::DeadBush,Item::DeadBush,"DeadBush",89,89,89,BiomeMaterial::Plant},
        {Block::LilyPad,Item::LilyPad,"LilyPad",90,90,90,BiomeMaterial::Plant},
        {Block::RedMushroom,Item::RedMushroom,"RedMushroom",91,91,91,BiomeMaterial::Plant},
        {Block::BrownMushroom,Item::BrownMushroom,"BrownMushroom",92,92,92,BiomeMaterial::Plant},
        {Block::MushroomStem,Item::MushroomStem,"MushroomStem",93,93,93,BiomeMaterial::Stone},
        {Block::RedMushroomBlock,Item::RedMushroomBlock,"RedMushroomBlock",94,94,94,BiomeMaterial::Stone},
        {Block::BrownMushroomBlock,Item::BrownMushroomBlock,"BrownMushroomBlock",95,95,95,BiomeMaterial::Stone},
    }};
    return entries;
}
