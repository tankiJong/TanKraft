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
    NUM_PHY_MODE,
  };

  Entity();
  virtual ~Entity() = default;

  virtual void onUpdate();
  virtual void onRender();

  Transform& transform() { return mTransform; }
  ePhysicsMode physicsMode() const { return mPhysicsMode; }
  void physicsMode(ePhysicsMode mode) { mPhysicsMode = mode; }
  void speed(vec3 speed) { mSpeed = speed; }
  span<CollisionSphere> collisions() { return { mCollisions.data(), mCollisionCounts };}
  span<const CollisionSphere> collisions() const { return { mCollisions.data(), mCollisionCounts };}
  bool possessed = false;

  static constexpr float kAccelerationScale = 50.f;
  static constexpr float kMaxAcceleration = 100.f;
  static constexpr float kMaxSpeed = 10.f;

protected:
  void addWillpower(const vec3& power);
  ePhysicsMode mPhysicsMode = PHY_GHOST;
  Transform  mTransform;
  Camera* mCamera;
  
  std::array<CollisionSphere, 10>  mCollisions;
  uint mCollisionCounts;

  vec3 mSpeed;

  vec3 mWillpower;

  float mSpeedScale = 1.f;
};
