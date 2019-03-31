#include "Game.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Debug/Profile/Overlay.hpp"
#include "Game/World/World.hpp"
#include "Game/VoxelRenderer/VoxelRenderer.hpp"
#include "Game/Utils/Config.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Entity.hpp"
#include "Game/Gameplay/Player.hpp"

void Game::onInit() {
  mWorld = new World();
  mSceneRenderer = new VoxelRenderer();
  mSceneRenderer->setWorld(mWorld);
  mCameraController = new FollowCamera1Person(nullptr);

  Player* player = addEntity<Player>();
  Player* debugPlayer = addEntity<Player>();
  mPlayer = player;
  mInspector = debugPlayer;

  player->physicsMode(Entity::PHY_NORMAL);
  mInspector->physicsMode(Entity::PHY_GHOST);

  mCameraController->possessTarget(mPossessPlayer ? mPlayer : mInspector);
  {
    mPlayer->transform().localPosition() = vec3{10,-5,108};
    mInspector->transform().localPosition() = vec3{3,-5,108};

    mPlayer->transform().localRotation() = vec3{0,0,0};
    mInspector->transform().localRotation() = vec3{0,0,0};
    // TODO: look at is buggy
    // mCameraController->camera().lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
   
    mCameraController->speedScale(10);

    Debug::setCamera(&mCameraController->camera());
  }

  mWorld->onInit();
  mSceneRenderer->onLoad(*RHIDevice::get()->defaultRenderContext());

}

void Game::onInput() {
  if(mPossessPlayer) {
    mPlayer->onInput();
  } else {
    mInspector->onInput();
  }

  mCameraController->onInput();

  {
    mCameraController->speedScale((Input::Get().isKeyDown(KEYBOARD_SHIFT) ? 100.f : 10.f));

    bool possessPlayer = mPossessPlayer;
    float scale = mCameraController->speedScale();
    vec3 camPosition = mCameraController->camera().transform().position();
    vec3 camRotation = mCameraController->camera().transform().localRotation();
    ImGui::Begin("Control");
    ImGui::Checkbox("Enable raycast", &mEnableRaycast);
    ImGui::Checkbox("Possess Player", &possessPlayer);
    ImGui::SliderFloat("Camera speed", &scale, 1, 500, "%0.f", 10);
    ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
    ImGui::SliderFloat3("Camera Rotation", (float*)&mCameraController->camera().transform().localRotation(), -360, 360);
    ImGui::End();
    mCameraController->speedScale(scale);

    if(possessPlayer != mPossessPlayer) {
      if(possessPlayer) {
        mCameraController->possessTarget(mPlayer);
      } else {
        mCameraController->possessTarget(mInspector);
      }
      mPossessPlayer = possessPlayer;
    }
  }

  if(Input::Get().isKeyJustDown('R')) {
    mEnableRaycast = !mEnableRaycast;
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
  float dt = (float)GetMainClock().frame.second;
  mCameraController->onUpdate(dt);

  mWorld->onUpdate(mCameraController->camera().transform().position());

  updateEntities();
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
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.end,
                      1, 0, Rgba::gray, Rgba(50, 50, 50));
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba(128, 0, 0));
      Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.contact.position,
                      1, 0, Rgba::red, Rgba::white);
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba::red);
    } else {
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.end,
                      1, 0, Rgba::green, Rgba::green);
    }

    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
  }

  // Debug::drawPoint(mCameraController->camera().transfrom().position() + mCameraController->camera().transfrom().right(), .03f, 0);
  Debug::drawBasis(mCameraController->camera().transform().position() + mCameraController->camera().transform().right(),
                   vec3::right * .05f, vec3::up * .05f, vec3::forward * .05f, 0);

  for(Entity* e: mEntities) {
    e->onRender();
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

void Game::updateEntities() {
  for(Entity* e: mEntities) {
    e->onUpdate();
  }

  for(Entity* e: mEntities) {
    if(e->physicsMode() == Entity::PHY_GHOST) continue; 
    bool collided = mWorld->collide(e->collision);
    if(collided) {
      e->speed(vec3::zero);
    }
  }
}
