#include "FollowCamera.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Debug/Draw.hpp"
#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Debug/Log.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Game/World/Chunk.hpp"
#include "Game/Gameplay/Entity.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Utils/Config.hpp"

static const vec3 MAX_ACCELERATION{ 1.f };

static const vec2 MAX_ANGULAR_ACCELERATION{ 180.f };

FollowCamera::FollowCamera(Entity* target, eCameraMode mode)
  : mTarget(target)
  , mCameraMode(mode) {
  mCamera = new Camera();
  if(mTarget != nullptr) {
    mCamera->transform().parent() = &mTarget->transform();
  }

  mCamera->setCoordinateTransform(gGameCoordsTransform);
  mCamera->transform().setRotationOrder(ROTATION_YZX);
  mCamera->setProjectionPrespective(70, 3.f*CLIENT_ASPECT, 3.f, .9f, Config::kMaxActivateDistance);
}

void FollowCamera::possessTarget(Entity* target) {
  if(mTarget != nullptr) {
    mTarget->possessed = false;
    mTarget->mCamera = nullptr;
  }
  mTarget = target;

  if(target == nullptr) {
    vec3 position = mCamera->transform().position();
    Euler rotation = mCamera->transform().rotation();
    mCamera->transform().parent() = nullptr;
    mCamera->transform().localRotation() = rotation;
    mCamera->transform().localPosition() = position;
  } else {
    mCamera->transform().parent() = &mTarget->transform();
    mTarget->possessed = true;
    mTarget->mCamera = mCamera;
  }

}

FollowCamera& FollowCamera::operator=(FollowCamera&& rhs) {
  mCamera = rhs.mCamera;
  mTarget = rhs.mTarget;
  mWorld = rhs.mWorld;

  rhs.mCamera = nullptr;

  return *this;
}

FollowCamera::FollowCamera() {
  SAFE_DELETE(mCamera);
}

void FollowCamera1Person::onInput() {

  if (Input::Get().isKeyDown(MOUSE_MBUTTON)) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition();
    if(deltaMouse.x > deltaMouse.y) deltaMouse.y = 0;
    if(deltaMouse.y > deltaMouse.x) deltaMouse.x = 0;
    addForce(-mCamera->transform().up() * deltaMouse.x / (float)GetMainClock().frame.second);
    addForce(mCamera->transform().right() * deltaMouse.y / (float)GetMainClock().frame.second);
  }
  if(Input::Get().isMouseLocked()) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition(true);
    addAngularForce(deltaMouse * 1000.f / (float)GetMainClock().frame.second);
  }


}

void FollowCamera1Person::onUpdate(float dt) {
  
  // shifting
  {
    vec3 acceleration = mForce;
    clamp(acceleration, -MAX_ACCELERATION, MAX_ACCELERATION);

    mMoveSpeed += acceleration;
    mCamera->translate(mMoveSpeed * dt * speedScale());
  }

  {
    vec2 angularAcce = mAngularForce;
    // clamp(angularAcce, -MAX_ANGULAR_ACCELERATION, MAX_ANGULAR_ACCELERATION);

    mAngularSpeed += angularAcce * dt;
    Euler angle{ 0, -mAngularSpeed.y * dt, mAngularSpeed.x * dt };
    
    mCamera->rotate(angle);

    mCamera->transform().localRotation().y
     = clamp(mCamera->transform().localRotation().y,
                  -85.f, 85.f);

    mCamera->transform().localPosition().z
     = clamp<float>(mCamera->transform().localPosition().z, 0, Chunk::kSizeZ);
  }

  // transform.localRotation() = mCamera->transfrom().localRotation();
  // Transform t;
  // Debug::drawBasis(t, 0);
  // ImGui::gizmos(mCamera, transform, ImGuizmo::ROTATE);
  // mat44 view = mCamera->view(), proj = mCamera->projection();
  // mat44 trans = transform.localToWorld();
  // ImGuizmo::DrawCube((float*)&view, (float*)&proj, (float*)&trans);
  // mCamera->transfrom().localRotation() = vec3::zero;

  // antanuation
  mMoveSpeed = vec3::zero;
  mForce = vec3::zero;
  
  mAngularSpeed = vec3::zero;
  mAngularForce = vec3::zero;
}

void FollowCamera1Person::speedScale(float scale) {
  mSpeedScale = scale;
}

void FollowCamera1Person::addForce(const vec3& force) {

  // assume the mass of the camera is 1
  mForce += force;

}

void FollowCamera1Person::addAngularForce(const vec2& force) {
  mAngularForce += force;
}

vec3 FollowCamera1Person::speed() const {
  return mMoveSpeed * mSpeedScale;
}

void FollowCameraOverSholder::onInput() {
  float dt = (float)GetMainClock().frame.second;

  if(Input::Get().isMouseLocked()) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition(true);
    mRotateAroundY += deltaMouse.y * 10 / dt;
    mRotateAroundZ += deltaMouse.x * 10 / dt;
  }

  // ImGui::Begin("Camera Control"); {
  //   ImGui::DragFloat("rotate y", &mRotateAroundY);
  //   ImGui::DragFloat("rotate z", &mRotateAroundZ);
  //   ImGui::DragFloat3("local position", (float*)&mCamera->transform().localPosition());
  //   ImGui::DragFloat3("local rotation", (float*)&mCamera->transform().localRotation());
  // } ImGui::End();
}

void FollowCameraOverSholder::onUpdate(float dt) {
  vec3 position;

  mRotateAroundY = clamp(mRotateAroundY, -30.f, 30.f);

  position.x = mDistanceFromTarget * cosDegrees(mRotateAroundY) * cosDegrees(mRotateAroundZ);
  position.y = mDistanceFromTarget * cosDegrees(mRotateAroundY) * sinDegrees(mRotateAroundZ);
  position.z = mDistanceFromTarget * sinDegrees(mRotateAroundY);

  mat44 t = mat44::translation(position);

  vec3 newx = (-position).normalized();
  float dot = vec3{0,0,1}.dot(newx);
  vec3 newy;
  vec3 newz;

  if(equal(dot, -1.f) || equal(dot, 1.f)) {
    newz = newx.cross(vec3{0, 1, 0}).normalized();
    newy = newz.cross(newx).normalized();
  } else {
    newy = vec3{0,0,1}.cross(newx).normalized();
    newz = newx.cross(newy);
  }

  mat44 r(
    vec4{newx, 0},
    vec4{newy, 0},
    vec4{newz, 0}
    );
  t = t * r;

  mCamera->transform().setlocalTransform(t);
  

}
