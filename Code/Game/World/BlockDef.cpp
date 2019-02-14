#include "BlockDef.hpp"
#include "Engine/Math/Primitives/uvec2.hpp"
#include "Engine/Math/Primitives/aabb2.hpp"

#define CI(x,y) spriteCoordsToIndex(x,y)

std::array<BlockDef, BlockDef::kTotalBlockDef> BlockDef::BlockDefs = {
  BlockDef{ "air", {CI(0,0), CI(0,0), CI(0,0)}},
  BlockDef{ "grass", {CI(21,0), CI(3,3), CI(4, 3)}},
  BlockDef{ "dust", {CI(4, 3), CI(4, 3), CI(4, 3)}},
  BlockDef{ "stone", {CI(1, 4), CI(1, 4), CI(1, 4)}},
};

#undef CI

aabb2 BlockDef::uvs(eFace face) const {

  uvec2 coords = spriteIndexToCoords(mSpriteIndex[face]);

  vec2 mins{kSpritesheetUnitU * coords.x, kSpritesheetUnitV * coords.y + kSpritesheetUnitV};
  vec2 maxs = mins + vec2{ kSpritesheetUnitU, -kSpritesheetUnitV };

  return { mins, maxs };
}

BlockDef* BlockDef::name(std::string_view defName) {
  for(auto& blockDef: BlockDefs) {
    if(blockDef.mName == defName) return &blockDef;
  }
  return nullptr;
}

uvec2  BlockDef::spriteIndexToCoords(uint index) {
  auto result = std::div(long(index), long(kSpritesheetUnitCountX));
  return {uint(result.rem), uint(result.quot)};
}
