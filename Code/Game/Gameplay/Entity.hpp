#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Transform.hpp"
#include "Game/Gameplay/Collision.hpp"

class Camera;

class Entity {
  friend class FollowCamera;
public:
  enum ePhysicsMode {
    PHY_NORMAL,
    PHY_FLY,
    PHY_GHOST,
  };

  Entity();
  virtual ~Entity() = default;

  virtual void onUpdate() = 0;
  virtual void onRender();

  Transform& transform() { return mTransform; }
  ePhysicsMode physicsMode() const { return mPhysicsMode; }
  void physicsMode(ePhysicsMode mode) { mPhysicsMode = mode; }
  void speed(vec3 speed) { mSpeed = speed; }

  bool possessed = false;
  CollisionSphere  collision;

  static constexpr float kAccelerationScale = 50.f;

protected:
  void accelerate(const vec3& force);
  ePhysicsMode mPhysicsMode = PHY_GHOST;
  Transform  mTransform;
  Camera* mCamera;
  
  vec3 mAcceleration;
  vec3 mSpeed;
};
