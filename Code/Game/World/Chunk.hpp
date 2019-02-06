#pragma once
#include "Engine/Core/common.hpp"
#include "Game/World/Block.hpp"
#include "Engine/Math/Primitives/ivec3.hpp"
#include "Engine/Math/Primitives/ivec2.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"

class Mesh;

using BlockIndex = uint16_t;

class BlockCoords: public ivec3 {
public:
  vec3 centerPosition() const { return vec3{};};
  BlockIndex toIndex() const;
  static BlockCoords fromIndex(BlockIndex index) { return BlockCoords{}; };
};

class ChunckCoords: public ivec2 {
public:
  using ivec2::ivec2;
  using ivec2::operator=;

  bool operator>(const ChunckCoords& rhs) const;
  bool operator<(const ChunckCoords& rhs) const {
    return !(*this > rhs || *this == rhs);
  }

  vec3 pivotPosition();
};

struct ChunckRenderData {
  Mesh* mesh = nullptr;
  mat44 model;
};

class Chunk {
public:
  static constexpr float kBlockDim = 1.f;
  
  static constexpr BlockIndex kSizeBitX = 4;
  static constexpr BlockIndex kSizeBitY = 4;
  static constexpr BlockIndex kSizeBitZ = 8;

  static constexpr uint kTotalBlockCount = 1 << (kSizeBitX + kSizeBitY + kSizeBitZ);

  static constexpr BlockIndex kSizeX = 1 << kSizeBitX;
  static constexpr BlockIndex kSizeY = 1 << kSizeBitY;
  static constexpr BlockIndex kSizeZ = 1 << kSizeBitZ;

  static constexpr BlockIndex kSizeMaskX = ~((~0) << kSizeBitX);
  static constexpr BlockIndex kSizeMaskY = ((1 << (kSizeBitX+kSizeBitY)) - 1) ^ kSizeMaskX;
  static constexpr BlockIndex kSizeMaskZ = (~0) ^ (kSizeMaskX | kSizeMaskY);

  void onUpdate();

protected:
  void reconstructMesh();
  Block mBlocks[kTotalBlockCount]; // 0xffff
  ChunckCoords mCoords;
  Mesh* mMesh = nullptr;
  Mesher mMesher;
  bool mIsDirty = true;
};
