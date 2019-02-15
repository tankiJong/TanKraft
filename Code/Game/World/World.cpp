#include "World.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"

#include "Game/VoxelRenderer/VoxelRenderer.hpp"

#include "Game/World/Chunk.hpp"
#include "Game/Utils/Config.hpp"
#include "imgui/imgui_internal.h"

void World::onInit() {

  if(sChunkActivationVisitingPattern.empty()) reconstructChunkVisitingPattern();

  mCamera.setCoordinateTransform(gGameCoordsTransform);
  // mCamera.transfrom().setCoordinateTransform(gGameCoordsTransform);
  mCamera.transfrom().setRotationOrder(ROTATION_YZX);
  mCamera.transfrom().localPosition() = vec3{-10,0,120};
  mCamera.transfrom().localRotation() = vec3{0,0,0};

  // TODO: look at is buggy
  // mCamera.lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
  mCamera.setProjectionPrespective(70, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, Config::kMinDeactivateDistance * 1.4f);
 
  mCameraController.speedScale(10);

  Debug::setCamera(&mCamera);
  Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
  Debug::drawBasis(Transform());

  // activateChunk({0,0});
  // activateChunk({-1,2});
  // activateChunk({0,3});
  // activateChunk({0,2});
  // activateChunk({1,2});
}

void World::onInput() {

  mCameraController.onInput();

  {
    // ImGui::Begin("Light Control");
    // ImGui::SliderFloat("Light Intensity", &intensity, 0, 100);
    // ImGui::SliderFloat3("Light color", (float*)&color, 0, 1);
    // ImGui::End();
    float scale = mCameraController.speedScale();
    vec3 camPosition = mCamera.transfrom().position();
    vec3 camRotation = mCamera.transfrom().localRotation();
    ImGui::Begin("Camera Control");
    ImGui::SliderFloat("Camera speed", &scale, 1, 500, "%0.f", 10);
    ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
    ImGui::SliderFloat3("Camera Rotation", (float*)&mCamera.transfrom().localRotation(), -360, 360);
    ImGui::End();
    mCameraController.speedScale(scale);
  }
}

void World::onUpdate() {
  float dt = (float)GetMainClock().frame.second;
  mCameraController.onUpdate(dt);

  updateChunks();
  manageChunks();
}

void World::onRender(VoxelRenderer& renderer) const {
  renderer.setCamera(mCamera);

  for(auto& [coords, chunk]: mActiveChunks) {
    if(chunk == nullptr) continue;
    renderer.issueChunk(chunk);
  }

  renderer.onRenderFrame(*RHIDevice::get()->defaultRenderContext());
}

bool World::activateChunk(ChunkCoords coords) {
  Chunk* chunk = allocChunk(coords);
  chunk->onInit();
  registerChunkToWorld(chunk);
  return true;
}

bool World::deactivateChunk(ChunkCoords coords) {
  Chunk* chunk = unregisterChunkFromWorld(coords);
  if(chunk == nullptr) {
    return false;
  }
  chunk->onDestroy();
  freeChunk(chunk);
  return true;
}

void World::registerChunkToWorld(Chunk* chunk) {
#ifdef _DEBUG
  {
    auto kv = mActiveChunks.find(chunk->coords());
    EXPECTS(kv == mActiveChunks.end() || 
            kv->second == nullptr);
  }
#endif

  mActiveChunks[chunk->coords()] = chunk;
}

owner<Chunk*> World::unregisterChunkFromWorld(ChunkCoords coords) {
  auto iter = mActiveChunks.find(coords);
  if(iter == mActiveChunks.end()) {
    return nullptr;
  };
  Chunk* chunk = iter->second;
  iter->second = nullptr;

  return chunk;
}

Chunk* World::findChunk(ChunkCoords coords) {
  auto iter = mActiveChunks.find(coords);
  return iter == mActiveChunks.end() ? nullptr : iter->second;
}

void World::updateChunks() {

  for(auto& [coords, chunk]: mActiveChunks) {
    if(chunk != nullptr) chunk->onUpdate();
  }
  
}

void World::manageChunks() {

  uint activatedChunkCount = 0;
  ChunkCoords playerChunkCoords = ChunkCoords::fromWorld(playerPosition());
  
  for(auto idx = sChunkActivationVisitingPattern.begin(); idx != sChunkActivationVisitingPattern.end(); ++idx) {
  
    ChunkCoords coords = playerChunkCoords + *idx;
  
    Chunk* chunk = findChunk(coords);
    if(chunk == nullptr) {
      activateChunk(coords);
      activatedChunkCount++;
    }
  
    if(activatedChunkCount == Config::kMaxChunkActivatePerFrame) break;
  }

  uint reconstructedMeshCount = 0;
  for(auto idx = sChunkActivationVisitingPattern.begin(); idx != sChunkActivationVisitingPattern.end(); ++idx) {

    ChunkCoords coords = playerChunkCoords + *idx;

    Chunk* chunk = findChunk(coords);
    if(chunk != nullptr && chunk->isDirty()) {
      chunk->reconstructMesh();
      reconstructedMeshCount++;
    }

    if(reconstructedMeshCount == Config::kMaxChunkReconstructMeshPerFrame) break;
  }

  uint deactivatedChunkCount = 0;
  for(auto idx = sChunkDeactivationVisitingPattern.begin(); idx != sChunkDeactivationVisitingPattern.end(); ++idx) {
  
    ChunkCoords coords = playerChunkCoords + *idx;
    if(deactivateChunk(coords)) {
      deactivatedChunkCount++;
    }
  
    if(deactivatedChunkCount == Config::kMaxChunkDeactivatePerFrame) break;
  }

}

vec3 World::playerPosition() {
  return mCamera.transfrom().position();
}

std::vector<ChunkCoords> World::sChunkActivationVisitingPattern{};
std::vector<ChunkCoords> World::sChunkDeactivationVisitingPattern{};
void World::reconstructChunkVisitingPattern() {
  int halfRange = (int)floor(Config::kMinDeactivateDistance / float(max(Chunk::kSizeY, Chunk::kSizeX)));

  int activateChunkDist = (int)floor(Config::kMaxActivateDistance / float(max(Chunk::kSizeY, Chunk::kSizeX)));

  EXPECTS(halfRange > 0);

  sChunkActivationVisitingPattern.reserve(halfRange * 2);
  sChunkDeactivationVisitingPattern.reserve(halfRange * 2);

  for(int j = -halfRange; j < halfRange; ++j) {
    for(int i = -halfRange; i < halfRange; ++i) {
      ivec2 coord{i, j};
      if(coord.magnitude2() <= activateChunkDist) {
        sChunkActivationVisitingPattern.emplace_back(coord);
      } else {
        sChunkDeactivationVisitingPattern.emplace_back(coord);
      }
    }
  }

  std::sort(sChunkActivationVisitingPattern.begin(), 
            sChunkActivationVisitingPattern.end(), 
            [](const ivec2& lhs, const ivec2& rhs) {
    return lhs.magnitude2() < rhs.magnitude2();
  });

  std::sort(sChunkDeactivationVisitingPattern.begin(), 
            sChunkDeactivationVisitingPattern.end(), 
            [](const ivec2& lhs, const ivec2& rhs) {
    return lhs.magnitude2() > rhs.magnitude2();
  });

  ENSURES(sChunkActivationVisitingPattern[0] == ivec2::zero);
}
