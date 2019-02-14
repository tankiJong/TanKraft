#pragma once
#include "Engine/Core/common.hpp"

class uvec2;
class aabb2;

class BlockDef {
public:
  static constexpr uint kTotalBlockDef = 4;
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
  BlockDef(std::string_view name, const std::array<uint, NUM_FACE>& spriteIndexs)
    : mName(name)
    , mSpriteIndex{spriteIndexs} {}

  aabb2 uvs(eFace face) const;

  static std::array<BlockDef, kTotalBlockDef> BlockDefs;
  static BlockDef* name(std::string_view defName);
  static constexpr uint spriteCoordsToIndex(uint x, uint y) { return x + y * (uint)kSpritesheetUnitCountX; }

  static uvec2 spriteIndexToCoords(uint index);

protected:

  std::string mName;
  std::array<uint, NUM_FACE> mSpriteIndex = { 0 };

};
