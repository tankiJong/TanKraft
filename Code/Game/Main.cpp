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
#include "Engine/Renderer/ImmediateRenderer.hpp"
#include "Engine/Renderer/SceneRenderer/SceneRenderer.hpp"
#include "Engine/Renderer/Renderable/Renderable.hpp"
#include "Engine/Graphics/Program/Material.hpp"
#include "Engine/Framework/Light.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/File/FileSystem.hpp"
#include "Engine/Application/Application.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Game/CameraController.hpp"
#include "Engine/Graphics/Model/BVH.hpp"
#include "Engine/Event/EventEmitter.hpp"
#include "Engine/Debug/Log.hpp"

#define SCENE_BUNNY
// #define SCENE_1
// #define SCENE_BOX
static float SCENE_SCALE = .002f;

// -------------------------  constant ------------------------------
constexpr uint frameCount = 2;
// ------------------------------------------------------------------


// GraphicsState::sptr_t GraphicsState;
// RootSignature::sptr_t rootSig;


Mesh* mesh;


Light mLight;



class GameApplication: public Application {

public:

  void onInit() override;

  void onInput() override;

  void onRender() const override;

  void onStartFrame() override;

  void onDestroy() override;

  float foo(float a, float b) {
    return a + b;
  }
protected:
  Camera* mCamera = nullptr;
  CameraController* cameraController = nullptr;
  SceneRenderer* sceneRenderer = nullptr;

  S<RHIDevice> mDevice;
  S<RHIContext> mContext;
  RenderScene scene{};
  Renderable meshRenderable{};

  BVH* mBvh;

  EventEmitter mEmitter;
};

auto lambdaFoo = [](float a, float b) {
    mat44 mat = mat44::makeScale2D(a, b);
    Log::logf("%f", a + b);
  };
void GameApplication::onInit() {
  sceneRenderer = new SceneRenderer(scene);

  mEmitter.on("foo", lambdaFoo);

  mCamera = new Camera();
  mCamera->lookAt({ 2, -2, -2 }, { -0.278000f, 0.273000f, 0.800000f });
  mCamera->setProjectionPrespective(19.5, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, 1500.000000f);

  mDevice = RHIDevice::get();
  mContext = mDevice->defaultRenderContext();

  uint w = Window::Get()->bounds().width();
  uint h = (uint)Window::Get()->bounds().height();

  // main pass
  {
    Mesher ms;
    ms.begin(DRAW_TRIANGES, false);

#ifdef SCENE_BUNNY
    ms.obj("/Data/model/bunny2.obj");
    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });
    ms.quad(SCENE_SCALE * vec3{ 0.0f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 0.0f, 500.f });  // floor
    
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f, 500.f });  // back wall
    
    ms.quad(
      SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 0.0f },
      SCENE_SCALE * vec3{ 0.0f, 500.f,   0.0f });  // ceiling
    ms.color(vec4{ 0.14, 0.45, 0.091, 1.f }); // G
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f });  // right wall
    
    
    ms.color(vec4{ 0.63, 0.065, 0.05, 1.f }); // R
    ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,   0.0f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f,   0.0f });  // left wall
#endif
#ifdef SCENE_BOX
    // ms.cube(vec3(30.f, 100.f, 0), vec3(200.f));

    // ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f, 0.f },
    //         SCENE_SCALE * vec3{ 0.0f, 548.8f, 0.f },
    //         SCENE_SCALE * vec3{ 556.0f, 548.8f, 0.f },
    //         SCENE_SCALE * vec3{ 549.6f,   0.0f, 0.f });  // back wall
    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });

    ms.cube(
      SCENE_SCALE * (vec3{ 82.f, 0.f, 114.f } +vec3{ 160.f, 0, 0 }),
      SCENE_SCALE * vec3{ 150.f, 145.f, 150.f },
      (vec3(240, 0, 65) - vec3(82, 0, 114)).normalized(),
      vec3::up,
      (vec3(130, 0, 272) - vec3(82, 0, 114)).normalized()); // short cube

    ms.cube(
      SCENE_SCALE * (vec3{ 265.f, 0, 406.f } +vec3{ -190.f, 0, -30.f }),
      SCENE_SCALE * vec3{ 160.f, 300.f, 150.f },
      (vec3(314, 0, 247) - vec3(265.f, 0, 406.f)).normalized(),
      vec3::up,
      (vec3(423, 0, 456) - vec3(265.f, 0, 406.f)).normalized()); // tall cube

    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });
    ms.quad(SCENE_SCALE * vec3{ 0.0f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 0.0f, 500.f });  // floor

    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f, 500.f });  // back wall

    ms.quad(
      SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 0.0f },
      SCENE_SCALE * vec3{ 0.0f, 500.f,   0.0f });  // ceiling
    ms.color(vec4{ 0.14, 0.45, 0.091, 1.f }); // G
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f });  // right wall


    ms.color(vec4{ 0.63, 0.065, 0.05, 1.f }); // R
    ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,   0.0f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f,   0.0f });  // left wall
#endif
#ifdef SCENE_1
    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });

    ms.cube(
      SCENE_SCALE * (vec3{ 150.f, 190.f, 130.f } +vec3{ 160.f, 0, 0 }),
      SCENE_SCALE * vec3{ 50.f, 50.f, 50.f },
      (vec3(240, 0, 65) - vec3(82, 0, 114)).normalized(),
      vec3::up,
      (vec3(130, 0, 272) - vec3(82, 0, 114)).normalized()); // short cube

    ms.cube(
      SCENE_SCALE * (vec3{ 265.f, 0, 406.f } +vec3{ -190.f, 0, -30.f }),
      SCENE_SCALE * vec3{ 160.f, 300.f, 150.f },
      (vec3(314, 0, 247) - vec3(265.f, 0, 406.f)).normalized(),
      vec3::up,
      (vec3(423, 0, 456) - vec3(265.f, 0, 406.f)).normalized()); // tall cube

    ms.cube(
      SCENE_SCALE * (vec3{ 130.f, 50.f, 114.f } +vec3{ 160.f, 0, 0 }),
      SCENE_SCALE * vec3{ 150.f, 100.f, 150.f },
      (vec3(240, 0, 65) - vec3(82, 0, 114)).normalized(),
      vec3::up,
      (vec3(130, 0, 272) - vec3(82, 0, 114)).normalized()); // short cube

    ms.color(Rgba::yellow);
    ms.quad(SCENE_SCALE * vec3{ 0.0f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 0.0f, 500.f });  // floor

    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f, 500.f });  // back wall

    ms.quad(
      SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
      SCENE_SCALE * vec3{ 500.f, 500.f, 0.0f },
      SCENE_SCALE * vec3{ 0.0f, 500.f,   0.0f });  // ceiling
    ms.color(vec4{ 0.14, 0.45, 0.091, 1.f }); // G
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f });  // right wall


    ms.color(vec4{ 0.63, 0.065, 0.05, 1.f }); // R
    ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,   0.0f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f,   0.0f });  // left wall
    // ms.color(Rgba::white);
    // ms.quad(vec3::zero, vec3::right, vec3::forward, SCENE_SCALE * vec2{ 500, 500 });
    // // ms.quad(SCENE_SCALE * vec3{ 0.0f, 0.0f, 0.0f },
    // //         SCENE_SCALE * vec3{ 500.f, 0.0f, 0.0f },
    // //         SCENE_SCALE * vec3{ 500.f, 0.0f, 500.f },
    // //         SCENE_SCALE * vec3{ 0.0f, 0.0f, 500.f });
    //
    // ms.color(Rgba::red);
    // ms.cube(SCENE_SCALE * vec3{ 100.f, 25.f, 100.f }, SCENE_SCALE * vec3::one * 50);
    //
    // ms.color(Rgba::yellow);
    // ms.cube(SCENE_SCALE * vec3{ 50.f, 0, 50.f }, SCENE_SCALE * vec3{ 30, 30, 30 },
    //         vec3{ 1, 0, -1 }.normalized(), vec3::up, vec3{ 1, 0, 1 }.normalized());
#endif
    // ms.quad(vec3(-30, 10, 0), -vec3::forward, vec3::up, vec2(20.f));
    ms.end();
    // ms.begin(DRAW_TRIANGES, false);

    // ms.end();
    // ms.begin(DRAW_TRIANGES, false);
    // ms.color(Rgba(20, 20, 200));
    // ms.quad(vec3::zero, vec3::right, vec3::forward, vec2(300.f));
    // // ms.sphere(vec3(0, 10.f, 0), 10.f, 15, 15);
    // ms.end();

    // buildMeshDataForCompute(ms);

    mBvh = ms.createBVH(15);
    mesh = ms.createMesh<vertex_lit_t>();
    meshRenderable.mesh() = mesh;
    meshRenderable.transform() = new Transform();

    Material* mat = new Material();
    mat->init();
    meshRenderable.material(*mat);

    mLight.asSpotLight(5.f, 10.f, 1.f);

    scene.add(meshRenderable);
    scene.set(*mCamera);
    scene.set(mBvh);
    scene.add(mLight);
  }

  // #ifdef SCENE_BOX
  vec3 position = SCENE_SCALE * vec3{ 250, 250, -1500 };
  mCamera->lookAt(position, position + vec3::forward);
  // #endif
  // #ifdef SCENE_1
  //   vec3 position = SCENE_SCALE * vec3{ -250, 100, -250 };
  //   mCamera->lookAt(position, SCENE_SCALE * vec3{ 250, 0, 250 });
  // #endif

  cameraController = new CameraController(*mCamera);
  cameraController->speedScale(1);
  mLight.transform.localPosition() = SCENE_SCALE * vec3{ 250.f, 497.8f, 250.f };

  Debug::setCamera(mCamera);
  // Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
  Debug::setDepth(Debug::DEBUG_DEPTH_ENABLE);
  // mBvh->render();
  sceneRenderer->onLoad(*mContext);
}

void GameApplication::onInput() {
  float dt = GetMainClock().frame.second;
  mEmitter.emit("foo", 1.0f, 3.2f);
  mEmitter.off("foo", lambdaFoo);

  static float frameAvgSec = 0.f;
  frameAvgSec = frameAvgSec * .95 + GetMainClock().frame.second * .05;
  Window::Get()->setTitle(Stringf("Tanki - Hybird Renderer. Frame time: %.0f ms",
                                  float(frameAvgSec * 1000.0)).c_str());
  cameraController->onInput();
  cameraController->onUpdate(dt);

  vec2 rotation = vec2::zero;
  if (Input::Get().isKeyDown(MOUSE_RBUTTON)) {
    rotation = Input::Get().mouseDeltaPosition(true) * 180.f * dt;
  }

  static float intensity = 5.f;
  static vec3 color = vec3::one;
  {
    ImGui::Begin("Light Control");
    ImGui::SliderFloat("Light Intensity", &intensity, 0, 100);
    ImGui::SliderFloat3("Light color", (float*)&color, 0, 1);
    ImGui::End();
    float scale = cameraController->speedScale();
    ImGui::Begin("Camera Control");
    ImGui::SliderFloat("Camera speed", &scale, 0, 10);
    ImGui::End();
    cameraController->speedScale(scale);
  }

  ImGui::gizmos(*mCamera, mLight.transform, ImGuizmo::TRANSLATE);
  mLight.asPointLight(intensity, vec3{ 2.f, 0, 0.f }, color);
}

void GameApplication::onRender() const {
  RHIDevice::get()->defaultRenderContext()->beforeFrame();
  sceneRenderer->onRenderFrame(*mContext);
}

void GameApplication::onStartFrame() {

}

void GameApplication::onDestroy() {
  delete sceneRenderer;
  mDevice->cleanup();
};


//-----------------------------------------------------------------------------------------------
int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR commandLineString, int) {
  GameApplication app;

  while (app.runFrame());

  return 0;
}

