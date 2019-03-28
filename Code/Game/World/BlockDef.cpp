#include "BlockDef.hpp"
#include "Engine/Math/Primitives/uvec2.hpp"
#include "Engine/Math/Primitives/aabb2.hpp"
#include "Game/World/Block.hpp"
#include "Engine/Debug/ErrorWarningAssert.hpp"
#include "Engine/Renderer/Shader/ShaderInterop.h"

#define CI(x,y) spriteCoordsToIndex(x,y)

std::array<BlockDef, BlockDef::kTotalBlockDef> BlockDef::sBlockDefs = {
  BlockDef{0, false, 0, "air", {CI(0,0), CI(0,0), CI(0,0)}},
  BlockDef{1, true, 0, "grass", {CI(21,0), CI(3,3), CI(4, 3)}},
  BlockDef{2, true, 0,"dust", {CI(4, 3), CI(4, 3), CI(4, 3)}},
  BlockDef{3, true, 0, "stone", {CI(1, 4), CI(1, 4), CI(1, 4)}},
  BlockDef{4, true, 0xf, "light", {CI(3, 13), CI(3, 13), CI(3, 13)}}
};

#undef CI

RHIBuffer::sptr_t BlockDef::sBlockDefBuffer = nullptr;



BlockDef::BlockDef() {
  for(uint face = 0; face < NUM_FACE; face++) {
    uvec2 coords = spriteIndexToCoords(mSpriteIndex[face]);

    vec2 mins{kSpritesheetUnitU * coords.x, kSpritesheetUnitV * coords.y + kSpritesheetUnitV};
    vec2 maxs = mins + vec2{ kSpritesheetUnitU, -kSpritesheetUnitV };

    mSpriteUVs[face] = { mins, maxs };
  }
}

BlockDef::BlockDef(block_id_t id, bool opaque, uint8_t emissiveAmount, std::string_view name, const std::array<uint, NUM_FACE>& spriteIndexs)
: mTypeId(id), mOpaque(opaque), mEmissiveAmount(emissiveAmount), mName(name), mSpriteIndex{spriteIndexs} {
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
  // EXPECTS(id < kTotalBlockDef);
  return &sBlockDefs[id];
}

uvec2  BlockDef::spriteIndexToCoords(uint index) {
  auto result = std::div(long(index), long(kSpritesheetUnitCountX));
  return {uint(result.rem), uint(result.quot)};
}

Block BlockDef::instantiate() const {
  Block block;
  block.mType = mTypeId;
  block.mBitFlags = mOpaque ? 0x1 : 0x0;

  return block;
}

struct BlockDefGPU {
  uint id;
  float emission;
  float opaque;
  float2 ___padding0;
  aabb2 uvs[4];
};

void BlockDef::init() {
  std::array<BlockDefGPU, kTotalBlockDef> defs;
  memset(defs.data(), 0, sizeof(defs));

  for(uint i = 0; i < kTotalBlockDef; ++i) {
    BlockDefGPU& gdef = defs[i];
    BlockDef& def = sBlockDefs[i];

    gdef.id = def.id();
    gdef.emission = (float)def.emissive() / (float)(UINT8_MAX - 1) * 100;
    gdef.opaque = def.opaque() ? 1 : 0;
    memcpy(gdef.uvs, def.mSpriteUVs.data(), sizeof(gdef.uvs));
  }

  sBlockDefBuffer = RHIBuffer::create(sizeof(defs), RHIResource::BindingFlag::ShaderResource, RHIBuffer::CPUAccess::None, defs.data());

  NAME_RHIRES(sBlockDefBuffer);
}
