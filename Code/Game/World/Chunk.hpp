#pragma once
#include <utility>
#include "Engine/Core/common.hpp"
#include "Game/World/Block.hpp"
#include "Engine/Math/Primitives/ivec3.hpp"
#include "Engine/Math/Primitives/ivec2.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"
#include "Engine/Math/Primitives/aabb3.hpp"
#include "Engine/Graphics/RHI/Texture.hpp"

class Chunk;
class Mesh;
class World;
class ChunkCoords;
struct aabb3;

using BlockIndex = uint16_t;


class BlockCoords: public ivec3 {
public:
  using ivec3::ivec3;

  BlockCoords() {}
  BlockCoords(const ivec3& copy): ivec3(copy) {}
  operator ivec3&() { return *this;}
  operator const ivec3&() const { return *this; }

  vec3 centerPosition() const;
  BlockIndex toIndex() const;

  static BlockCoords fromWorld(const Chunk* chunk, const vec3& world);
  static BlockCoords fromIndex(BlockIndex index);
  static aabb3 blockBounds(Chunk* chunk, BlockIndex index);
  static aabb3 blockBounds(Chunk* chunk, const BlockCoords& coords);
  static vec3  blockCenterPosition(Chunk* chunk, BlockIndex index);
  static BlockIndex toIndex(BlockIndex x, BlockIndex y, BlockIndex z);
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

namespace std {
  template<>
  struct hash<ChunkCoords> {
    size_t operator()(const ChunkCoords& c) const noexcept {
      std::hash<int> hasher;
      return  (hasher(c.x) ^ hasher(c.y) << 1);
    };
  };
}

class Chunk {
  Chunk() = default;
public:
  static Chunk sInvalidChunk;
  
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

  Chunk(ChunkCoords coords);

  ~Chunk();
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
    Iterator() = default;
    void step(eNeighbor dir);
    void step(const ChunkCoords& deltaCoords);
    Iterator operator+(const ChunkCoords& deltaCoords) const;

    bool valid() const { return self != &sInvalidChunk; }

    Chunk* operator->() const { return self; }
    Chunk& operator*() const { return *self; }

    bool operator==(const Iterator& rhs) const { return self == rhs.self; }
    bool operator!=(const Iterator& rhs) const { return self != rhs.self; }

    Chunk* chunk() const { return self; }
  protected:
    Iterator(Chunk& chunk): self(&chunk) {}
    Chunk* self = &sInvalidChunk;

  };

  class BlockIter {
    friend class Chunk;

  public:
    BlockIter(Chunk& c, BlockCoords crds);
    BlockIter(Chunk& c, BlockIndex index);
    BlockIter next(const BlockCoords& deltaCoords) const;
    BlockIter nextNegX() const;
    BlockIter nextPosX() const;
    BlockIter nextNegY() const;
    BlockIter nextPosY() const;
    BlockIter nextNegZ() const;
    BlockIter nextPosZ() const;
    void stepNegX();
    void stepPosX();
    void stepNegY();
    void stepPosY();
    void stepPosZ();
    void stepNegZ();
    void step(const BlockCoords& deltaCoords);
    bool valid() const;

    void reset(BlockDef& def);
    void dirtyLight();

    aabb3 bounds() const {
      return BlockCoords::blockBounds(chunk.self, blockIndex);
    }
    BlockCoords coords() const {
      return BlockCoords::fromIndex(blockIndex);
    }

    vec3 centerPosition() const {
      return BlockCoords::blockCenterPosition(chunk.self, blockIndex);
    }

    BlockIndex index() const {
      return blockIndex;
    }
    BlockIter operator+(const BlockCoords& deltaCoords) const;

    Block& operator*() const;
    Block* operator->() const;
    Block* block() const;
    bool operator==(const BlockIter& rhs) const { return chunk == rhs.chunk && blockIndex == rhs.blockIndex; };
    bool operator!=(const BlockIter& rhs) const { return !(*this == rhs); };
    Iterator chunk;
  protected:

    BlockIndex blockIndex;
  };

  void onInit();
  void afterRegisterToWorld();
  void onUpdate();

  Mesh* mesh() const { return mMesh; }

  void onDestroy();
  ChunkCoords coords() const { return mCoords; };

  aabb3 bounds() const { return mBounds; }
  const Block& block(uint index) const { return mBlocks[index]; }
  Block& block(BlockIndex index) { return mBlocks[index]; }

  bool isDirty() const { return mIsDirty; }
  void setDirty() { mIsDirty = true; };

  bool reconstructMesh();

  Iterator iterator();
  BlockIter blockIter(const BlockCoords& coords) { return { *this, coords }; };
  BlockIter blockIter(BlockIndex index) { return { *this, index }; };
  BlockIter blockIter(const vec3& world);

  Iterator neighbor(eNeighbor loc) const { return { *mNeighbors[loc] }; }

  void setNeighbor(eNeighbor loc, Chunk& chunk) { mNeighbors[loc] = &chunk; }

  void onRegisterToWorld(World* world);
  void onUnregisterFromWorld();

  size_t serialize(byte_t* data, size_t maxWrite) const;
  void deserialize(byte_t* data, size_t maxRead);

  void markSavePending() { mSavePending =true;}

  bool valid() const;
  bool invalid() const { return !valid(); }

  static Iterator invalidIter() { return { sInvalidChunk }; }

  void resetBlock(BlockIndex index, BlockDef& def);

  const Texture3::sptr_t& gpuVolume() { return mChunkGPUData == nullptr ? sInvalidChunk.mChunkGPUData : mChunkGPUData; }
  void rebuildGpuMetaData();
protected:

  void addBlock(const BlockIter& block, const vec3& pivot);
  void markBlockLightDirty(const BlockIter& block);

  void generateBlocks();
  void initLights();
  bool neighborsLoaded() const;


  std::array<Block, kTotalBlockCount> mBlocks; // 0xffff
  ChunkCoords mCoords;
  std::array<Chunk*, NUM_NEIGHBOR> mNeighbors 
    { &sInvalidChunk, &sInvalidChunk, &sInvalidChunk, &sInvalidChunk, };
  Mesher mMesher;
  World* mOwner = nullptr;
  aabb3 mBounds;
  
  owner<Mesh*> mMesh = nullptr;
  Texture3::sptr_t mChunkGPUData = nullptr;

  bool mSavePending = false;
  bool mIsDirty = true;

};

inline BlockIndex BlockCoords::toIndex() const {
  BlockIndex index = 0;
  // EXPECTS(x >=0 && x < Chunk::kSizeX);
  // EXPECTS(y >=0 && y < Chunk::kSizeY);
  // EXPECTS(z >=0 && z < Chunk::kSizeZ);

  index = (x & Chunk::kSizeMaskX)
        | ((y << Chunk::kSizeBitX) & Chunk::kSizeMaskY)
        | ((z << (Chunk::kSizeBitX + Chunk::kSizeBitY)) & Chunk::kSizeMaskZ);

  return index;
}

inline BlockCoords BlockCoords::fromIndex(BlockIndex index) {
  return { Chunk::kSizeMaskX & index, 
          (Chunk::kSizeMaskY & index) >> Chunk::kSizeBitX,
          (Chunk::kSizeMaskZ & index) >> (Chunk::kSizeBitX + Chunk::kSizeBitY) };
}

inline bool Chunk::valid() const {
  EXPECTS(this != nullptr);
  return this != &sInvalidChunk;
}

inline BlockIndex BlockCoords::toIndex(BlockIndex x, BlockIndex y, BlockIndex z) {
  return (x & Chunk::kSizeMaskX)
        | ((y << Chunk::kSizeBitX) & Chunk::kSizeMaskY)
        | ((z << (Chunk::kSizeBitX + Chunk::kSizeBitY)) & Chunk::kSizeMaskZ);
}
