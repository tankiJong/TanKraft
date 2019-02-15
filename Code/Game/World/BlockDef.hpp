#pragma once
#include "Engine/Core/common.hpp"

class uvec2;
class aabb2;

using block_id_t = uint8_t;
class BlockDef {
public:
  static constexpr block_id_t kTotalBlockDef = 4;
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
  BlockDef(block_id_t id, std::string_view name, const std::array<uint, NUM_FACE>& spriteIndexs)
    : mTypeId(id)
    , mName(name)
    , mSpriteIndex{spriteIndexs} {}

  aabb2 uvs(eFace face) const;
  block_id_t id() const { return mTypeId; }
  static BlockDef* get(std::string_view defName);
  static BlockDef* get(block_id_t id);
  static constexpr uint spriteCoordsToIndex(uint x, uint y) { return x + y * (uint)kSpritesheetUnitCountX; }

  static uvec2 spriteIndexToCoords(uint index);

protected:
  static std::array<BlockDef, kTotalBlockDef> sBlockDefs;
  block_id_t mTypeId;
  std::string mName;
  std::array<uint, NUM_FACE> mSpriteIndex = { 0 };

};
