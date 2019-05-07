#include "Game.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Debug/Profile/Overlay.hpp"
#include "Game/World/World.hpp"
#include "Game/VoxelRenderer/VoxelRenderer.hpp"
#include "Game/Gameplay/Entity.hpp"
#include "Game/Gameplay/Player.hpp"
#include "Engine/Async/Job.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/vary.hpp"
#include "Engine/Event/EventEmitter.hpp"
#include "Engine/Core/any_func.hpp"

struct NameProperty {
  
};
void dummy(int a, float b) {
  DebugBreak();
}

void Game::onInit() {

  // `vary` is like std::any, but cannot perform casting inside the class
  // it is with runtime check for `get()`, can be compiled out
  int integer = 1;
  float fp = 2.f;
  char c = 'Y';
  vary v1;
  v1.set(integer);
  vary v2;
  v1.set(v2);
  v1.set(2.f);
  v1.set(c);

  c << v1;
  c = v1.get<char>();

  // a callable container which can store function of any type, 
  // the only limitation is you cannot store lambda with too big sizeof(capture)
  any_func func1(&dummy);
  any_func func2([](int a, char b) {});
  any_func func3([this](int a) {
    dummyMethod(1, 3.f);
  });
  any_func func4(this, &Game::dummyMethod);

  EventEmitter emitter;
  emitter.on("hello", &dummy);
  emitter.on("hello", this, &Game::dummyMethod);
  emitter.on("hello", [this](int a, float b) {
    DebugBreak();
  });

  //emitter.emit("hello", 1, 6.f);

  emitter.off("hello", this, &Game::dummyMethod);

  VaryMap kvs;


  emitter.on("vmaptest", [](VaryMap& kvs) {
    kvs.set("int", 2);
  });

  emitter.on("vmaptest", [](VaryMap& kvs) {
    kvs.set("mat44", mat44::identity);
  });

  emitter.on("vmaptest", [](VaryMap& kvs) {
    int xxx;
    kvs.get("int", xxx);
    mat44 mat = kvs.get<mat44>("mat44");
  });

  emitter.emit("vmaptest", kvs);
  //emitter.emit("hello", 2, 3.f);

  // provide runtime check for operator(), which can be compiled out
  //func1(1, 2.f);
  //func2(1, '2');
  //func3(1);
  //func4(3, 2.f);

  // `any_func` with cached params, 
  // the params has to be copy constructable, since my usage of this type has to cache off the context
  closure cl1(&dummy, 1, 2.f);
  closure cl2(this, &Game::dummyMethod, 3, 2.f);
  closure cl3([]() {
    DebugBreak();
  });
  //cl1();
  //cl2();

  // They can all be packed into array!
  std::vector<vary> anyThing;
  std::vector<any_func> anyFunctions;
  std::vector<closure> anyClosures;

  auto a = [] {};
  mWorld = new World();
  mSceneRenderer = new VoxelRenderer();
  mSceneRenderer->setWorld(mWorld);
  mCameraController = new FollowCamera1Person(nullptr);
  mCameraController->world(mWorld);

  Player* player = addEntity<Player>();
  Player* debugPlayer = addEntity<Spectator>();
  mPlayer = player;
  mSpectator = debugPlayer;

  player->physicsMode(Entity::PHY_GHOST);
  mSpectator->physicsMode(Entity::PHY_GHOST);
  mCameraController->possessTarget(mPossessPlayer ? mPlayer : mSpectator);
  mCameraController->camera().transform().localPosition() = {.0f, .0f, 1.65f - .5f};
  {
    mPlayer->transform().localPosition() = vec3{10,-5,108};
    mSpectator->transform().localPosition() = vec3{3,-5,108};

    mPlayer->transform().localRotation() = vec3{0,0,0};
    mSpectator->transform().localRotation() = vec3{0,0,0};
    // TODO: look at is buggy
    // mCameraController->camera().lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
   
    // mCameraController->speedScale(10);

    Debug::setCamera(&mCameraController->camera());
  }

  mWorld->onInit();
  mSceneRenderer->onLoad(*RHIDevice::get()->defaultRenderContext());
  Chunk::sInvalidChunk.onInit();
  //Chunk::sInvalidChunk.rebuildGpuMetaData();

}

void Game::onInput() {
  if(mPossessPlayer) {
    mPlayer->onInput();
  } else {
    mSpectator->onInput();
  }

  if(Input::Get().isKeyJustDown(KEYBOARD_F2) && mPossessPlayer) {
    eCameraMode mode = (eCameraMode)((mCameraController->cameraMode() + 1) % NUM_CAMERA_MODE);
    setCamera(mode, mCameraController->target());
  }

  if(Input::Get().isKeyJustDown(KEYBOARD_F3) && mPossessPlayer) {
    Entity::ePhysicsMode mode 
      = (Entity::ePhysicsMode)
        ((mCameraController->target()->physicsMode() + 1) % Entity::NUM_PHY_MODE);

    mCameraController->target()->physicsMode(mode);
  }

  mCameraController->onInput();

  {
    bool possessPlayer = mPossessPlayer;
    vec3 camPosition = mCameraController->camera().transform().position();
    vec3 camRotation = mCameraController->camera().transform().localRotation();
    ImGui::Begin("Control");
    ImGui::Checkbox("Enable raycast", &mEnableRaycast);
    ImGui::Checkbox("Possess Player", &possessPlayer);
    ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
    ImGui::SliderFloat3("Camera Rotation", (float*)&mCameraController->camera().transform().localRotation(), -360, 360);
    ImGui::End();

    if(Input::Get().isKeyJustDown(KEYBOARD_F4)) {
      possessPlayer = !possessPlayer;
    }

    if(possessPlayer != mPossessPlayer) {
      if(possessPlayer) {
        mCameraController->possessTarget(mPlayer);
      } else {
        mCameraController->possessTarget(mSpectator);
      }
      mPossessPlayer = possessPlayer;
    }
  }

  if(Input::Get().isKeyJustDown('R')) {
    mEnableRaycast = !mEnableRaycast;
  }

  
  if(Input::Get().isKeyJustDown('L')) {
    Input::Get().toggleMouseLockCursor();
  }
  
  if(Input::Get().isKeyJustDown(MOUSE_LBUTTON)) {
    if(mPlayerRaycast.impacted()) {
      BlockDef* air = BlockDef::get(0);
      mPlayerRaycast.contact.block.reset(*air);
      mPlayerRaycast.contact.block.dirtyLight();
      mPlayerRaycast.contact.block.chunk->markSavePending();
    }
  }

  if(Input::Get().isKeyJustDown(MOUSE_RBUTTON)) {
    if(mPlayerRaycast.impacted() && mPlayerRaycast.contact.distance > 0) {

      BlockDef* light = nullptr;

      if(Input::Get().isKeyDown(KEYBOARD_CONTROL)) {
        light = BlockDef::get("light");
      } else {
        light = BlockDef::get("stone");
      }
      auto iter = mPlayerRaycast.contact.block.next(mPlayerRaycast.contact.normal);
      iter.reset(*light);
      iter.dirtyLight();
      iter.chunk->markSavePending();
    }
  }
  
  if(mEnableRaycast) {
    
    mPlayerRaycast = mWorld->raycast(mCameraController->camera().transform().position(), 
                              mCameraController->camera().transform().right().normalized(), 8);
  }

  mWorld->onInput();


  
}

void Game::onUpdate() {
  Job::Consumer consumer;
  Job::category_t cats[] = {Job::CAT_MAIN_THREAD};
  consumer.init(cats);
  consumer.consumeFor(5);

  float dt = (float)GetMainClock().frame.second;
  vec3 playerPosition = mCameraController->camera().transform().position();
  mWorld->onUpdate(playerPosition);
  updateEntities();

  mCameraController->onUpdate(dt);

  if(mPossessPlayer) {
    mSpectator->transform() =  mPlayer->transform();
  }
  mSceneRenderer->updatePlayerPosition(playerPosition); 
}

void Game::onRender() const {
  mSceneRenderer->setCamera(mCameraController->camera());
  mWorld->onRender(*mSceneRenderer);
  
  if(mPlayerRaycast.impacted()) {
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);

    aabb3 innerbounds = mPlayerRaycast.contact.block.bounds();
    innerbounds = { innerbounds.mins - vec3{.03f}, innerbounds.maxs + vec3{.03f}};
    Debug::drawCube(innerbounds, mat44::identity, true, 0, Rgba::white);

    {
      /*
       *     2 ----- 1
       *    /|      /|
       *   3 ----- 0 |
       *   | 6 ----|-5
       *   |/      |/             x  
       *   7 ----- 4         y___/
       */  
      vec3 v[8] = {
        vec3{ innerbounds.mins.x, innerbounds.mins.y, innerbounds.maxs.z },
        vec3{ innerbounds.maxs.x, innerbounds.mins.y, innerbounds.maxs.z },
        vec3{ innerbounds.maxs.x, innerbounds.maxs.y, innerbounds.maxs.z },
        vec3{ innerbounds.mins.x, innerbounds.maxs.y, innerbounds.maxs.z },

        vec3{ innerbounds.mins.x, innerbounds.mins.y, innerbounds.mins.z },
        vec3{ innerbounds.maxs.x, innerbounds.mins.y, innerbounds.mins.z },
        vec3{ innerbounds.maxs.x, innerbounds.maxs.y, innerbounds.mins.z },
        vec3{ innerbounds.mins.x, innerbounds.maxs.y, innerbounds.mins.z },
      };

      Rgba tintColor(50, 20, 20);
      if(mPlayerRaycast.contact.normal.x == 1) {
        Debug::drawQuad(v[5], v[6], v[2], v[1], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.x == -1) {
        Debug::drawQuad(v[7], v[4], v[0], v[3], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.y == 1) {
        Debug::drawQuad(v[6], v[7], v[3], v[2], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.y == -1) {
        Debug::drawQuad(v[5], v[4], v[0], v[1], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.z == 1) {
        Debug::drawQuad(v[3], v[0], v[1], v[2], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.z == -1) {
        Debug::drawQuad(v[6], v[5], v[4], v[7], 0, tintColor);
      }
    }
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_DISABLE);

  }

  if(!mEnableRaycast) {
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_DISABLE);
    if(mPlayerRaycast.impacted()) {
      Debug::drawLine(mPlayerRaycast.ray.start(), mPlayerRaycast.ray.end(),
                      1, 0, Rgba::gray, Rgba(50, 50, 50));
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba(128, 0, 0));
      Debug::drawLine(mPlayerRaycast.contact.position, mPlayerRaycast.contact.normal * .5f+ mPlayerRaycast.contact.position, 1, 0, Rgba::red, Rgba::red);
      Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
      Debug::drawLine(mPlayerRaycast.ray.start(), mPlayerRaycast.contact.position,
                      1, 0, Rgba::red, Rgba::white);
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba::red);
    } else {
      Debug::drawLine(mPlayerRaycast.ray.start(), mPlayerRaycast.ray.end(),
                      1, 0, Rgba::green, Rgba::green);
    }

    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
  }

  // Debug::drawPoint(mCameraController->camera().transfrom().position() + mCameraController->camera().transfrom().right(), .03f, 0);
  Debug::drawBasis(mCameraController->camera().transform().position() + mCameraController->camera().transform().right(),
                   vec3::right * .05f, vec3::up * .05f, vec3::forward * .05f, 0);

  //for(Entity* e: mEntities) {
  //  e->onRender();
  //}


  switch(mPlayer->physicsMode()) { 
    case Entity::PHY_NORMAL: 
      Debug::drawText2("Physics Mode Walk", 15.f, vec2{10.f, 10.f}, 0);
    break;
    case Entity::PHY_FLY: 
      Debug::drawText2("Physics Mode Fly", 15.f, vec2{10.f, 10.f}, 0);
    break;
    case Entity::PHY_GHOST: 
      Debug::drawText2("Physics Mode Ghost", 15.f, vec2{10.f, 10.f}, 0);
    break;
  }

  if(mPossessPlayer) {
    switch(mCameraController->cameraMode()) {
      case CAMERA_1_PERSON: 
        Debug::drawText2("Camera Mode 1st Person", 15.f, vec2{10.f, 30.f}, 0);
      break;
      case CAMERA_OVER_SHOULDER: 
        Debug::drawText2("Camera Mode Over Shoulder", 15.f, vec2{10.f, 30.f}, 0);
      break;
      case CAMERA_FIXED_ANGLE: 
        Debug::drawText2("Camera Mode Fixed Angle", 15.f, vec2{10.f, 30.f}, 0);
      break;
    }
  } else {
      Debug::drawText2("Spectator Mode", 15.f, vec2{10.f, 30.f}, 0);
  }
}

void Game::postUpdate() {
  mWorld->onEndFrame();
}

void Game::onDestroy() {
  mWorld->onDestroy();
  SAFE_DELETE(mWorld);
  SAFE_DELETE(mSceneRenderer);
}

void Game::dummyMethod(int a, float b) {
  DebugBreak();
}

void Game::setCamera(eCameraMode mode, Entity* target) {

  FollowCamera* camera = nullptr;
  switch(mode) { 
    case CAMERA_1_PERSON:
      camera = new FollowCamera1Person(target);
    break;
    case CAMERA_OVER_SHOULDER:
      camera = new FollowCameraOverSholder(target);
    break;
    case CAMERA_FIXED_ANGLE: 
      camera = new FollowCameraFixedAngle(target);
    break;
  }

  if(mCameraController != nullptr) {
    *camera = std::move(*mCameraController);
    SAFE_DELETE(mCameraController);
    mCameraController = camera;
  }

  mCameraController->camera().transform().localPosition() = {.0f, .0f, 1.65f - .5f};

}

void Game::updateEntities() {
  for(Entity* e: mEntities) {
    e->onUpdate();
  }

  for(Entity* e: mEntities) {
    if(e->physicsMode() == Entity::PHY_GHOST) continue; 
    bool collided = mWorld->collide(e->collisions()[0]);
    if(collided) {
      e->speed(vec3::zero);
    }
  }
}
