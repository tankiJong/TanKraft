#include "World.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"

#include "Game/VoxelRenderer/VoxelRenderer.hpp"

#include "Game/World/Chunk.hpp"
void World::onInit() {
  mCamera.setCoordinateTransform(gGameCoordsTransform);
  // mCamera.transfrom().setCoordinateTransform(gGameCoordsTransform);
  mCamera.transfrom().setRotationOrder(ROTATION_YZX);
  mCamera.transfrom().localPosition() = vec3{-10,0,120};
  mCamera.transfrom().localRotation() = vec3{0,0,0};

  // TODO: look at is buggy
  // mCamera.lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
  mCamera.setProjectionPrespective(70, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, 1500.000000f);
 
  mCameraController.speedScale(10);

  Debug::setCamera(&mCamera);
  Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
  Debug::drawBasis(Transform());

  activateChunk({0,0});
  activateChunk({-1,2});
  activateChunk({0,3});
  activateChunk({0,2});
  activateChunk({1,2});
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

  for(auto& [coords, chunk]: mActiveChunks) {
    chunk->onUpdate();
  }
}

void World::onRender(VoxelRenderer& renderer) const {
  renderer.setCamera(mCamera);

  for(auto& [coords, chunk]: mActiveChunks) {
    renderer.issueChunk(chunk);
  }

  renderer.onRenderFrame(*RHIDevice::get()->defaultRenderContext());
}

void World::activateChunk(ChunkCoords coords) {
  Chunk* chunk = allocChunk(coords);
  registerChunkToWorld(chunk);
}

void World::registerChunkToWorld(Chunk* chunk) {
  {
    auto kv = mActiveChunks.find(chunk->coords());
    EXPECTS(kv == mActiveChunks.end());
  }
  mActiveChunks[chunk->coords()] = chunk;
}
