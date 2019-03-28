#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/aabb2.hpp"
#include "Engine/Graphics/RHI/TypedBuffer.hpp"

class uvec2;
class aabb2;

class Block;

using block_id_t = uint8_t;
class BlockDef {
public:
  static constexpr size_t kTotalBlockDef = size_t(block_id_t(-1)) + 1u;
  static constexpr float kSpritesheetSizeX = 1024;
  static constexpr float kSpritesheetSizeY = 1024;

  static constexpr float kSpritesheetUnitCountX = 32;
  static constexpr float kSpritesheetUnitCountY = 32;

  static constexpr float kSpritesheetUnitU = 1.f / kSpritesheetUnitCountX;
  static constexpr float kSpritesheetUnitV = 1.f / kSpritesheetUnitCountY;
  enum eFace {
    FACE_TOP,
    FACE_SIDE,
    FACE_BTM,
    NUM_FACE,
  };

  BlockDef();
  BlockDef(block_id_t id, bool opaque, uint8_t emissiveAmount, std::string_view name, const std::array<uint, NUM_FACE>& spriteIndexs);

  const aabb2& uvs(eFace face) const {
    return mSpriteUVs[face];
  };
  block_id_t id() const { return mTypeId; }
  bool opaque() const { return mOpaque; }
  uint8_t emissive() const { return mEmissiveAmount; }

  static BlockDef* get(std::string_view defName);
  static BlockDef* get(block_id_t id);
  static constexpr uint spriteCoordsToIndex(uint x, uint y) { return x + y * (uint)kSpritesheetUnitCountX; }

  static uvec2 spriteIndexToCoords(uint index);
  Block instantiate() const;
  static void init();
  static RHIBuffer::sptr_t sBlockDefBuffer;
protected:
  static std::array<BlockDef, kTotalBlockDef> sBlockDefs;
  block_id_t mTypeId = 255;
  std::string mName = "invalid";
  uint8_t mEmissiveAmount = 0;
  bool mOpaque = false;
  std::array<uint, NUM_FACE> mSpriteIndex = { 255 };
  std::array<aabb2, NUM_FACE> mSpriteUVs;

};
