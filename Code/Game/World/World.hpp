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
protected:
  std::map<ChunckCoords, Chunk*> mChunks;
  Camera mCamera;
  CameraController mCameraController;
};
