#pragma once
#include "Engine/Core/common.hpp"
#include <map>
#include "Game/World/Chunk.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Game/CameraController.hpp"

class Chunk;
class VoxelRenderer;

class World {
public:

  struct raycast_result_t {
    struct contact_t {
      vec3 position           = vec3::zero;
      vec3 normal             = vec3::zero;
      float fraction          = 1;
      float distance          = 0;
      Chunk::BlockIter block  = Chunk::BlockIter{ nullptr, 0 };
    };

    vec3 start        = vec3::zero;
    vec3 end          = vec3::zero;
    vec3 dir          = vec3::zero;
    float maxDist     = 0;
    contact_t contact;      

    bool impacted() const { return contact.fraction < 1; }
  };
  World(): mCameraController(mCamera) {}

  void onInit();
  void onInput();
  void onUpdate();
  void onRender(VoxelRenderer& renderer) const;
  void onDestroy();

  bool activateChunk(ChunkCoords coords);
  bool deactivateChunk(ChunkCoords coords);

  Chunk* findChunk(const ChunkCoords& coords);
  Chunk* findChunk(const vec3& worldPosition);
  
  const Chunk* findChunk(const ChunkCoords& coords) const;

  raycast_result_t raycast(const vec3& start, const vec3& dir, float maxDist);

  raycast_result_t& playerRaycastResult() { return mPlayerRaycast; }
protected:

  Chunk* allocChunk(ChunkCoords coords) { return new Chunk(coords); }
  void freeChunk(Chunk* chunk) { delete chunk; } 
  void registerChunkToWorld(Chunk* chunk);
  owner<Chunk*> unregisterChunkFromWorld(const ChunkCoords& coords);

  void updateChunks();
  void manageChunks();

  vec3 playerPosition();
  std::unordered_map<ChunkCoords, Chunk*> mActiveChunks;
  Camera mCamera;
  CameraController mCameraController;
  raycast_result_t mPlayerRaycast;

  bool mEnableRaycast = true;
  static std::vector<ChunkCoords> sChunkActivationVisitingPattern;
  static std::vector<ChunkCoords> sChunkDeactivationVisitingPattern;
  static void reconstructChunkVisitingPattern();
};
