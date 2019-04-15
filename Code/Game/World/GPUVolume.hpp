#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/uvec3.hpp"
#include "Game/World/Chunk.hpp"

class GPUVolume {
public:

  static constexpr BlockIndex kTileSizeX = Chunk::kSizeX;
  static constexpr BlockIndex kTileSizeY = Chunk::kSizeY;
  static constexpr BlockIndex kTileSizeZ = Chunk::kSizeZ;

  void init(uint tileCountX, uint tileCountY, eTextureFormat format);
  void setWorld(const World* world) { mWorld = world; }
  void update(vec3 playerPosition);

  Texture3::sptr_t& volume() { return mVolume; }
  Texture3::sptr_t& visibilityVolume() { return mVisibilityVolume; }

protected:

  uvec3 voxelIndexOffset(uint tileIndexX, uint tileIndexY);
  uvec3 voxelIndexOffset(uvec2 tileIndex) { return voxelIndexOffset(tileIndex.x, tileIndex.y); }

  void updateSubVolumeAndMipmapsDetail() const;
  void updateSubVolumeAndMipmapsRough() const;
  const World* mWorld = nullptr;
  vec3 mPlayerPosition;
  uint mTileCountX;
  uint mTileCountY;
  uvec2 mPlayerTileIndex;

  Texture3::sptr_t mVolume;
  Texture3::sptr_t mVisibilityVolume;
};
