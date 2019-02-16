#include "BlockDef.hpp"
#include "Engine/Math/Primitives/uvec2.hpp"
#include "Engine/Math/Primitives/aabb2.hpp"

#define CI(x,y) spriteCoordsToIndex(x,y)

std::array<BlockDef, BlockDef::kTotalBlockDef> BlockDef::sBlockDefs = {
  BlockDef{0, false, "air", {CI(0,0), CI(0,0), CI(0,0)}},
  BlockDef{1, true, "grass", {CI(21,0), CI(3,3), CI(4, 3)}},
  BlockDef{2, true, "dust", {CI(4, 3), CI(4, 3), CI(4, 3)}},
  BlockDef{3, true, "stone", {CI(1, 4), CI(1, 4), CI(1, 4)}},
};

#undef CI

BlockDef::BlockDef(block_id_t id, bool opaque, std::string_view name, const std::array<uint, NUM_FACE>& spriteIndexs)
: mTypeId(id), mOpaque(opaque), mName(name), mSpriteIndex{spriteIndexs} {
  for(uint face = 0; face < NUM_FACE; face++) {
    uvec2 coords = spriteIndexToCoords(mSpriteIndex[face]);

    vec2 mins{kSpritesheetUnitU * coords.x, kSpritesheetUnitV * coords.y + kSpritesheetUnitV};
    vec2 maxs = mins + vec2{ kSpritesheetUnitU, -kSpritesheetUnitV };

    mSpriteUVs[face] = { mins, maxs };
  }
}

BlockDef* BlockDef::get(std::string_view defName) {
  for(auto& blockDef: sBlockDefs) {
    if(blockDef.mName == defName) return &blockDef;
  }
  return nullptr;
}

BlockDef* BlockDef::get(block_id_t id) {
  return &sBlockDefs[id];
}

uvec2  BlockDef::spriteIndexToCoords(uint index) {
  auto result = std::div(long(index), long(kSpritesheetUnitCountX));
  return {uint(result.rem), uint(result.quot)};
}
