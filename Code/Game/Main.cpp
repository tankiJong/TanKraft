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
#include "Game/Gameplay/FollowCamera.hpp"
#include "Game/VoxelRenderer/VoxelRenderer.hpp"
#include "Game/GameCommon.hpp"
#include "Game/World/World.hpp"
#include "Game/Utils/FileCache.hpp"
#include "Game/Game.hpp"
#include "Engine/Async/Job.hpp"

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
  void onEndFrame() override;
  void onDestroy() override;

protected:

  void renderStatisticsOverlay() const;
  // Camera* mCamera = nullptr;
  // CameraController* cameraController = nullptr;

  S<RHIDevice> mDevice;
  S<RHIContext> mContext;
  Game mGame;
};

void GameApplication::onInit() {

  SAFE_DELETE(gGameClock);
  gGameClock = GetMainClock().createChild();

  Job::startup(Job::NUM_CATEGORY);

  mDevice = RHIDevice::get();
  mContext = mDevice->defaultRenderContext();

  FileCache::get().init();

  Input::Get().mouseLockCursor(true);
  // Input::Get().mouseHideCursor(false);

  BlockDef::init();

  mGame.onInit();
}

void GameApplication::onInput() {
  static bool firstFrame = true;
  if(!firstFrame) {
    mGame.onInput();
  } else {
    firstFrame = !firstFrame;
    return;
  }
  float dt = (float)GetMainClock().frame.second;

  static float frameAvgSec = 0.f;
  frameAvgSec = frameAvgSec * .95f + dt * .05f;


  Window::Get()->setTitle(Stringf("Tanki - Tankraft. Frame time: %.0f ms",
                                  float(frameAvgSec * 1000.0)).c_str());

  mGame.onUpdate();

}

void GameApplication::onRender() const {
  mGame.onRender();
  renderStatisticsOverlay();
  // sceneRenderer->onRenderFrame(*mContext);
}

void GameApplication::onStartFrame() {

}

void GameApplication::onEndFrame() {
  mGame.postUpdate();
}

void GameApplication::onDestroy() {
  mGame.onDestroy();
  mDevice->cleanup();
}

void GameApplication::renderStatisticsOverlay() const {
  const float DISTANCE = 10.0f;
  static int corner = 0;

  bool   open       = true;
  ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - DISTANCE : DISTANCE,
                             (corner & 2) ? ImGui::GetIO().DisplaySize.y - DISTANCE : DISTANCE);
  ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
  
  ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
  ImGui::SetNextWindowBgAlpha(0.3f); // Transparent background
  
  if(ImGui::Begin("World Status Overlay",
                  &open,
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                  ImGuiWindowFlags_NoNav)) {

    ImGui::Text("World Status" "(right-click to change position)");
    ImGui::TextColored({.3f, .5f, 1.f, 1.f}, "World Time: %.4f", gGameClock->total.second / 86400.f);

    ImGui::Separator();
    
    auto rc = mGame.playerRaycastResult();
    if(rc.impacted()) {

      ivec3 coord = rc.contact.block.coords();
      BlockIndex index = rc.contact.block.index();
      ivec2 chunkCoords = rc.contact.block.chunk->coords();
      std::string seletedBlockInfo = "";

      ImGui::Text("Current Chunk (%s)", chunkCoords.toString().c_str());
      ImGui::Text("Block 0x%x(%i, %i, %i)", index, coord.x, coord.y, coord.z);

    } else {
      ImGui::Text("Current Chunk: ");
      ImGui::Text("Block ###");
    }

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
        if (ImGui::MenuItem("Top-right",    NULL, corner == 1)) corner = 1;
        if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
        if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
        ImGui::EndPopup();
    }

  }
  
  ImGui::End();

}

//-----------------------------------------------------------------------------------------------
int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR /*commandLineString*/, int) {
  GameApplication app;
  CurrentThread::setName("Main Thread");
  while (app.runFrame());

  return 0;
}

