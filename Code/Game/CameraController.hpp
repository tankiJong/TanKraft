#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/vec3.hpp"
#include "Engine/Math/Primitives/vec2.hpp"

class Camera;

class CameraController {
public:
  CameraController(Camera& cam): mCamera(cam) {
  }

  void onInput();
  void onUpdate(float dt);

  void speedScale(float scale);
  float speedScale() const { return mSpeedScale; };
  void addForce(const vec3& force);
  void addAngularForce(const vec2& force);

  vec3 speed() const;
protected:
  Camera& mCamera;
  vec3 mMoveSpeed;
  vec2 mAngularSpeed;
  vec3 mForce;
  vec2 mAngularForce;
  float mSpeedScale = 1;
};
