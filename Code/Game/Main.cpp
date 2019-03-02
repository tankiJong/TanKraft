#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <Windows.h>

#include "Engine/Core/type.h"
#include "Engine/File/Utils.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Engine/Graphics/RHI/RootSignature.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Graphics/Model/Vertex.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Application/Window.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"
#include "Engine/Renderer/SceneRenderer/SceneRenderer.hpp"
#include "Engine/Renderer/Renderable/Renderable.hpp"
#include "Engine/Framework/Light.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/File/FileSystem.hpp"
#include "Engine/Application/Application.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Game/CameraController.hpp"
#include "Game/VoxelRenderer/VoxelRenderer.hpp"
#include "Game/GameCommon.hpp"
#include "Game/World/World.hpp"
#include "Game/Utils/FileCache.hpp"

#define SCENE_BUNNY
// #define SCENE_1
// #define SCENE_BOX
static float SCENE_SCALE = .002f;

// -------------------------  constant ------------------------------
constexpr uint frameCount = 2;
// ------------------------------------------------------------------


// GraphicsState::sptr_t GraphicsState;
// RootSignature::sptr_t rootSig;







class GameApplication: public Application {

public:

  void onInit() override;

  void onInput() override;

  void onRender() const override;

  void onStartFrame() override;

  void onDestroy() override;

protected:
  // Camera* mCamera = nullptr;
  // CameraController* cameraController = nullptr;
  VoxelRenderer* sceneRenderer = nullptr;

  S<RHIDevice> mDevice;
  S<RHIContext> mContext;

  World* mWorld = nullptr;
};

void GameApplication::onInit() {
  sceneRenderer = new VoxelRenderer();

  // mCamera = new Camera();
  // mCamera->setCoordinateTransform(gGameCoordsTransform);
  // // mCamera->transfrom().setCoordinateTransform(gGameCoordsTransform);
  // mCamera->transfrom().localPosition() = vec3{0,0,0};
  // mCamera->transfrom().localRotation() = vec3{0,0,0};
  //
  // // TODO: look at is buggy
  // // mCamera->lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
  // mCamera->setProjectionPrespective(19.5, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, 1500.000000f);

  mDevice = RHIDevice::get();
  mContext = mDevice->defaultRenderContext();

  // uint w = (uint)Window::Get()->bounds().width();
  // uint h = (uint)Window::Get()->bounds().height();

  // cameraController = new CameraController(*mCamera);
  // cameraController->speedScale(10);

  
  // Debug::setDepth(Debug::DEBUG_DEPTH_ENABLE);
  // mBvh->render();

  // sceneRenderer->setCamera(*mCamera);
  sceneRenderer->onLoad(*mContext);

  FileCache::get().init();

  mWorld = new World();
  mWorld->onInit();
  sceneRenderer->setWorld(mWorld);
  Input::Get().mouseLockCursor(true);
  // Input::Get().mouseHideCursor(false);
}

void GameApplication::onInput() {
  static bool firstFrame = true;
  if(!firstFrame) {
    mWorld->onInput();
  } else {
    firstFrame = !firstFrame;
    return;
  }
  float dt = (float)GetMainClock().frame.second;

  static float frameAvgSec = 0.f;
  frameAvgSec = frameAvgSec * .95f + dt * .05f;

  std::string seletedBlockInfo = "";

  auto rc = mWorld->playerRaycastResult();
  if(rc.impacted()) {
    ivec3 coord = rc.contact.block.coords();
    BlockIndex index = rc.contact.block.index();
    ivec2 chunkCoords = rc.contact.block.chunk->coords();
    seletedBlockInfo = Stringf("Chunk(%s), Index: %x, Coords: %i, %i, %i", chunkCoords.toString().c_str(), index, coord.x, coord.y, coord.z);
  }
  Window::Get()->setTitle(Stringf("Tanki - Tankraft. Frame time: %.0f ms, %s",
                                  float(frameAvgSec * 1000.0), seletedBlockInfo.c_str()).c_str());
  // cameraController->onInput();
  // cameraController->onUpdate(dt);
  mWorld->onUpdate();
  // vec2 rotation = vec2::zero;
  // if (Input::Get().isKeyDown(MOUSE_RBUTTON)) {
  //   rotation = Input::Get().mouseDeltaPosition(true) * 180.f * dt;
  // }
  //
  // static float intensity = 5.f;
  // static vec3 color = vec3::one;
  // {
  //   // ImGui::Begin("Light Control");
  //   // ImGui::SliderFloat("Light Intensity", &intensity, 0, 100);
  //   // ImGui::SliderFloat3("Light color", (float*)&color, 0, 1);
  //   // ImGui::End();
  //   float scale = cameraController->speedScale();
  //   vec3 camPosition = mCamera->transfrom().position();
  //   vec3 camRotation = mCamera->transfrom().rotation();
  //   ImGui::Begin("Camera Control");
  //   ImGui::SliderFloat("Camera speed", &scale, 0, 20);
  //   ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
  //   ImGui::SliderFloat3("Camera Rotation", (float*)&mCamera->transfrom().localRotation(), -360, 360);
  //   ImGui::End();
  //   cameraController->speedScale(scale);
  // }
  
}

void GameApplication::onRender() const {
  mWorld->onRender(*sceneRenderer);
  // sceneRenderer->onRenderFrame(*mContext);
}

void GameApplication::onStartFrame() {

}

void GameApplication::onDestroy() {
  mWorld->onDestroy();
  SAFE_DELETE(mWorld);
  SAFE_DELETE(sceneRenderer);
  mDevice->cleanup();
}


//-----------------------------------------------------------------------------------------------
int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR /*commandLineString*/, int) {
  GameApplication app;

  while (app.runFrame());

  return 0;
}

