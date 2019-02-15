#pragma once
#include <utility>
#include "Engine/Core/common.hpp"
#include "Game/World/Block.hpp"
#include "Engine/Math/Primitives/ivec3.hpp"
#include "Engine/Math/Primitives/ivec2.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"

class Mesh;
struct aabb3;

using BlockIndex = uint16_t;

class BlockCoords: public ivec3 {
public:
  using ivec3::ivec3;

  vec3 centerPosition() const { return vec3{};};
  BlockIndex toIndex() const;

  static BlockCoords fromIndex(BlockIndex index);;
};

class ChunkCoords: public ivec2 {
public:
  using ivec2::ivec2;
  using ivec2::operator=;

  ChunkCoords(const ivec2& copy): ivec2(copy) {}
  bool operator>(const ChunkCoords& rhs) const;
  bool operator<(const ChunkCoords& rhs) const {
    return !(*this > rhs || *this == rhs);
  }

  vec3 pivotPosition();
  
  static ChunkCoords fromWorld(vec3 position);
};

class Chunk {
public:
  
  static constexpr BlockIndex kSizeBitX = 4;
  static constexpr BlockIndex kSizeBitY = 4;
  static constexpr BlockIndex kSizeBitZ = 8;

  static constexpr uint kTotalBlockCount = 1 << (kSizeBitX + kSizeBitY + kSizeBitZ);

  static constexpr BlockIndex kSizeX = 1 << kSizeBitX;
  static constexpr BlockIndex kSizeY = 1 << kSizeBitY;
  static constexpr BlockIndex kSizeZ = 1 << kSizeBitZ;

  static constexpr BlockIndex kSizeMaskX = BlockIndex(~((~0u) << kSizeBitX));
  static constexpr BlockIndex kSizeMaskY = BlockIndex(((1u << (kSizeBitX+kSizeBitY)) - 1u) ^ kSizeMaskX);
  static constexpr BlockIndex kSizeMaskZ = BlockIndex((~0u) ^ (kSizeMaskX | kSizeMaskY));

  Chunk(ChunkCoords coords): mCoords(std::move(coords)) {}

  void onInit();
  void onUpdate();

  Mesh* mesh() const { return mMesh; }

  void onDestroy();
  ChunkCoords coords() const { return mCoords; };

  aabb3 bounds();

  bool isDirty() const { return mIsDirty; }

  void reconstructMesh();
protected:

  void generateBlocks();

  Block mBlocks[kTotalBlockCount]; // 0xffff
  ChunkCoords mCoords;
  Mesh* mMesh = nullptr;
  Mesher mMesher;
  bool mIsDirty = true;
};
