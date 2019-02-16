#pragma once
#include <utility>
#include "Engine/Core/common.hpp"
#include "Game/World/Block.hpp"
#include "Engine/Math/Primitives/ivec3.hpp"
#include "Engine/Math/Primitives/ivec2.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"

class Mesh;
class World;
class ChunkCoords;
struct aabb3;

using BlockIndex = uint16_t;


class BlockCoords: public ivec3 {
public:
  using ivec3::ivec3;

  BlockCoords(const ivec3& copy): ivec3(copy) {}
  operator ivec3&() { return *this;}
  operator const ivec3&() const { return *this; }

  vec3 centerPosition() const;
  BlockIndex toIndex() const;

  static BlockCoords fromIndex(BlockIndex index);;
  static BlockIndex  stepIndex(BlockIndex index, BlockCoords delta, ChunkCoords* outChunkShift);
};

class ChunkCoords: public ivec2 {
public:
  using ivec2::ivec2;
  using ivec2::operator=;

  ChunkCoords() {};
  ChunkCoords(const ivec2& copy): ivec2(copy) {};
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

  enum eNeighbor: uint8_t {
    NEIGHBOR_POS_X,
    NEIGHBOR_NEG_X,
    NEIGHBOR_POS_Y,
    NEIGHBOR_NEG_Y,

    NUM_NEIGHBOR,
  };

  class Iterator {
    friend class Chunk;

  public:

    void step(eNeighbor dir);
    void step(ChunkCoords deltaCoords);
    Iterator operator+(ChunkCoords deltaCoords) const;

    bool valid() const { return self != nullptr; }

    Chunk* operator->() const { return self; }
    Chunk& operator*() const { return *self; }

    bool operator==(const Iterator& rhs) const { return self == rhs.self; }
    bool operator!=(const Iterator& rhs) const { return self != rhs.self; }
  protected:
    Iterator(Chunk* chunk): self(chunk) {}
    Chunk* self = nullptr;

  };

  class BlockIter {
    friend class Chunk;

  public:
    BlockIter next(BlockCoords deltaCoords) const;
    void step(BlockCoords deltaCoords);
    bool valid() const;
    BlockIter operator+(BlockCoords deltaCoords) const;

    Block& operator*() const;
    Block* operator->() const;

  protected:
    BlockIter(Chunk* c, BlockIndex idx);

    Iterator chunk;
    BlockIndex blockIndex;
    Block* block = nullptr;
  };

  void onInit(World* world);
  void onUpdate();

  Mesh* mesh() const { return mMesh; }

  void onDestroy();
  ChunkCoords coords() const { return mCoords; };

  aabb3 bounds();
  Block& block(BlockIndex index) { return mBlocks[index]; }

  bool isDirty() const { return mIsDirty; }

  bool reconstructMesh();

  Iterator iterator();
  BlockIter blockIter(BlockIndex index) { return { this, index }; };
  Iterator neighbor(eNeighbor loc) const { return { mNeighbors[loc] }; }

  void setNeighbor(eNeighbor loc, Chunk* chunk) { mNeighbors[loc] = chunk; }
protected:

  void generateBlocks();
  bool neighborsLoaded() const;
  Block mBlocks[kTotalBlockCount]; // 0xffff
  ChunkCoords mCoords;
  std::array<Chunk*, NUM_NEIGHBOR> mNeighbors;
  Mesh* mMesh = nullptr;
  Mesher mMesher;
  bool mIsDirty = true;
  World* mOwner;
};