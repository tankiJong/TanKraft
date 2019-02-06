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
  mCamera.transfrom().localPosition() = vec3{0,0,0};
  mCamera.transfrom().localRotation() = vec3{0,0,0};

  // TODO: look at is buggy
  // mCamera.lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
  mCamera.setProjectionPrespective(40, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, 1500.000000f);

  mCameraController.speedScale(10);

  Debug::setCamera(&mCamera);
  Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
}

void World::onInput() {
  float dt = GetMainClock().frame.second;

  mCameraController.onInput();
  mCameraController.onUpdate(dt);

  {
    // ImGui::Begin("Light Control");
    // ImGui::SliderFloat("Light Intensity", &intensity, 0, 100);
    // ImGui::SliderFloat3("Light color", (float*)&color, 0, 1);
    // ImGui::End();
    float scale = mCameraController.speedScale();
    vec3 camPosition = mCamera.transfrom().position();
    vec3 camRotation = mCamera.transfrom().localRotation();
    ImGui::Begin("Camera Control");
    ImGui::SliderFloat("Camera speed", &scale, 0, 20);
    ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
    ImGui::SliderFloat3("Camera Rotation", (float*)&mCamera.transfrom().localRotation(), -360, 360);
    ImGui::End();
    mCameraController.speedScale(scale);
  }
}

void World::onUpdate() {
  for(auto& [coords, chunk]: mChunks) {
    chunk->onUpdate();
  }
}

void World::onRender(VoxelRenderer& renderer) const {
  renderer.setCamera(mCamera);
  renderer.onRenderFrame(*RHIDevice::get()->defaultRenderContext());
}
