#pragma once
#include "Engine/Core/common.hpp"
#include <map>
#include "Game/World/Chunk.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Game/Gameplay/FollowCamera.hpp"
#include "Engine/Memory/RingBuffer.hpp"
#include "Game/Gameplay/Collision.hpp"

class Chunk;
class VoxelRenderer;

struct raycast_result_t {
  struct contact_t {
    vec3 position           = vec3::zero;
    vec3 normal             = vec3::zero;
    float fraction          = 1;
    float distance          = 0;
    Chunk::BlockIter block  = Chunk::BlockIter{ *Chunk::invalidIter().chunk(), 0 };
  };

  vec3 start        = vec3::zero;
  vec3 end          = vec3::zero;
  vec3 dir          = vec3::zero;
  float maxDist     = 0;
  contact_t contact;      

  bool impacted() const { return contact.fraction < 1; }
};

class World {
public:
  void onInit();
  void onInput();
  void onUpdate(const vec3& viewPosition);
  void onRender(VoxelRenderer& renderer) const;
  void onDestroy();

  bool activateChunk(ChunkCoords coords);
  bool deactivateChunk(ChunkCoords coords);

  Chunk* findChunk(const ChunkCoords& coords) const;
  Chunk* findChunk(const vec3& worldPosition) const;

  raycast_result_t raycast(const vec3& start, const vec3& dir, float maxDist) const;

  void submitDirtyBlock(const Chunk::BlockIter& block);

  owner<Mesh*> aquireDebugLightDirtyMesh() const;

  float currentLightningStrikeLevel() const { return mWeatherNoiseSample.front(); }
  float currentLightFlickLevel() const { return mFlameNoiseSample.front(); }

  void onEndFrame();

  bool collide(CollisionSphere& target) const;
  bool collide(span<CollisionSphere> target) const;

protected:

  Chunk* allocChunk(ChunkCoords coords) { return new Chunk(coords); }
  void freeChunk(Chunk* chunk) { delete chunk; } 
  void registerChunkToWorld(Chunk* chunk);
  owner<Chunk*> unregisterChunkFromWorld(const ChunkCoords& coords);
  vec3 viewPosition();

  void updateChunks();
  void manageChunks();

  void propagateLight(bool step);

  vec3 mCurrentViewPosition;
  std::unordered_map<ChunkCoords, Chunk*> mActiveChunks;

  std::deque<Chunk::BlockIter> mLightDirtyList;

  RingBuffer mWeatherNoiseSample;
  RingBuffer mFlameNoiseSample;

  static std::vector<ChunkCoords> sChunkActivationVisitingPattern;
  static std::vector<ChunkCoords> sChunkDeactivationVisitingPattern;
  static void reconstructChunkVisitingPattern();
};
