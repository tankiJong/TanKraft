#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/vec3.hpp"
#include "Engine/Math/Primitives/vec2.hpp"
#include "Engine/Graphics/Camera.hpp"

class Camera;
class Entity;
class World;

enum eCameraMode {
  CAMERA_UNDEFINED = -1,
  CAMERA_1_PERSON,
  CAMERA_OVER_SHOULDER,
  CAMERA_FIXED_ANGLE,
  NUM_CAMERA_MODE,
};

class FollowCamera {
public:
  FollowCamera(Entity* target, eCameraMode mode);

  virtual void onInput() = 0;
  virtual void onUpdate(float dt) = 0;
  virtual ~FollowCamera() = default;

  const Camera& camera() const { return *mCamera; }
  Camera& camera() { return *mCamera; }
  Entity* target() const { return mTarget; }
  void world(World* w) { mWorld = w; }
  void possessTarget(Entity* target);
  eCameraMode cameraMode() const { return mCameraMode; }

  FollowCamera& operator=(FollowCamera&& rhs);

  FollowCamera();
protected:

  owner<Camera*> mCamera;
  Entity* mTarget = nullptr;
  World* mWorld = nullptr;
  eCameraMode mCameraMode = CAMERA_UNDEFINED;
};



class FollowCamera1Person: public FollowCamera {
public:
  FollowCamera1Person(Entity* target)
    : FollowCamera(target, CAMERA_1_PERSON) {};

  void onInput() override;
  void onUpdate(float dt) override;

  void speedScale(float scale);
  float speedScale() const { return mSpeedScale; };
  void addForce(const vec3& force);
  void addAngularForce(const vec2& force);

  vec3 speed() const;

protected:
  vec3 mMoveSpeed;
  vec2 mAngularSpeed;
  vec3 mForce;
  vec2 mAngularForce;
  float mSpeedScale = 1;
};

class FollowCameraOverSholder: public FollowCamera {
public:
  FollowCameraOverSholder(Entity* target)
    : FollowCamera(target, CAMERA_OVER_SHOULDER) {}

  void onInput() override;
  void onUpdate(float dt) override;

protected:
  float mRotateAroundZ = -40.f;
  float mRotateAroundY = 30.f;
  float mDistanceFromTarget = 2.f;
};

class FollowCameraFixedAngle: public FollowCameraOverSholder {
public:
  FollowCameraFixedAngle(Entity* target)
    : FollowCameraOverSholder(target) {
    mCameraMode = CAMERA_FIXED_ANGLE;
    mDistanceFromTarget = 10.f;
  }

  void placeCamera(float rotateY, float rotateZ, float distance) {
    mRotateAroundY = rotateY;
    mRotateAroundZ = rotateZ;
    mDistanceFromTarget = distance;
  };
  void onInput() override {};
};