#pragma once
#include "Engine/Core/common.hpp"
#include <vector>
#include "Game/Gameplay/FollowCamera.hpp"
#include "Game/World/World.hpp"

class World;
class VoxelRenderer;

class Entity;
class Player;

class Game {
public:
  void onInit();
  void onInput();
  void onUpdate();
  void onRender() const;
  void postUpdate();
  void onDestroy();

  const World* world() const { return mWorld; }

  raycast_result_t playerRaycastResult() const { return mPlayerRaycast; }
protected:

  template<typename E>
  E* addEntity() {
    E* e = new E();
    mEntities.push_back(e);
    return e;
  }

  void updateEntities();

  World* mWorld = nullptr;
  VoxelRenderer* mSceneRenderer = nullptr;

  Player* mPlayer = nullptr;
  Player* mInspector = nullptr;

  std::vector<Entity*> mEntities;

  FollowCameraOverSholder* mCameraController = nullptr;
  raycast_result_t mPlayerRaycast;

  // for debug
  bool mEnableRaycast = true;
  bool mPossessPlayer = false;
};
