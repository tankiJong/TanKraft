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
  World(): mCameraController(mCamera) {}
  void onInit();
  void onInput();
  void onUpdate();
  void onRender(VoxelRenderer& renderer) const;

  bool activateChunk(ChunkCoords coords);
  bool deactivateChunk(ChunkCoords coords);

protected:

  Chunk* allocChunk(ChunkCoords coords) { return new Chunk(coords); }
  void freeChunk(Chunk* chunk) { delete chunk; } 
  void registerChunkToWorld(Chunk* chunk);
  owner<Chunk*> unregisterChunkFromWorld(ChunkCoords coords);

  Chunk* findChunk(ChunkCoords coords);
  void updateChunks();
  void manageChunks();

  vec3 playerPosition();
  std::map<ChunkCoords, Chunk*> mActiveChunks;
  Camera mCamera;
  CameraController mCameraController;

  static std::vector<ChunkCoords> sChunkActivationVisitingPattern;
  static std::vector<ChunkCoords> sChunkDeactivationVisitingPattern;
  static void reconstructChunkVisitingPattern();
};
