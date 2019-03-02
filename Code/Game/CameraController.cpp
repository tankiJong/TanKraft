#include "CameraController.hpp"
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

static const vec3 MAX_ACCELERATION{ 1.f };

static const vec2 MAX_ANGULAR_ACCELERATION{ 180.f };

void CameraController::onInput() {

  {
    if(Input::Get().isKeyDown('W')) {
      addForce({ mCamera.transform().right().xy().normalized(), 0 });
    }
    if(Input::Get().isKeyDown('S')) {
      addForce({ -mCamera.transform().right().xy().normalized(), 0 });
    }
  }

  {
    if (Input::Get().isKeyDown('D')) {
      addForce({ -mCamera.transform().up().xy().normalized(), 0 });
    }
    if (Input::Get().isKeyDown('A')) {
      addForce({ mCamera.transform().up().xy().normalized(), 0 });
    }
  }

  {
    if (Input::Get().isKeyDown('E')) {
      addForce({ 0,0, 1 });
    }
    if (Input::Get().isKeyDown('Q')) {
      addForce({ 0,0, -1 });
    }
  }

  if(Input::Get().isKeyJustDown(KEYBOARD_SPACE)) {
    mCamera.transform().localPosition() = vec3{-3, 0,0};
    mCamera.transform().localRotation() = vec3::zero;
  }

  if(Input::Get().isKeyJustDown('L')) {
    Input::Get().toggleMouseLockCursor();
  }

  if (Input::Get().isKeyDown(MOUSE_MBUTTON)) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition();
    addForce(-mCamera.transform().right() * deltaMouse.x / (float)GetMainClock().frame.second);
    addForce(mCamera.transform().up() * deltaMouse.y / (float)GetMainClock().frame.second);
  }
  if(Input::Get().isMouseLocked()) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition(true);
    addAngularForce(deltaMouse * 1000.f / (float)GetMainClock().frame.second);
  }


}

void CameraController::onUpdate(float dt) {
  
  // shifting
  {
    vec3 acceleration = mForce;
    clamp(acceleration, -MAX_ACCELERATION, MAX_ACCELERATION);

    mMoveSpeed += acceleration;
    mCamera.translate(mMoveSpeed * dt * speedScale());
  }

  {
    vec2 angularAcce = mAngularForce;
    // clamp(angularAcce, -MAX_ANGULAR_ACCELERATION, MAX_ANGULAR_ACCELERATION);

    mAngularSpeed += angularAcce * dt;
    Euler angle{ 0, -mAngularSpeed.y * dt, mAngularSpeed.x * dt };
    
    mCamera.rotate(angle);

    mCamera.transform().localRotation().y
     = clamp(mCamera.transform().localRotation().y,
                  -85.f, 85.f);

    mCamera.transform().localPosition().z
     = clamp<float>(mCamera.transform().localPosition().z, 0, Chunk::kSizeZ);
  }

  // transform.localRotation() = mCamera.transfrom().localRotation();
  // Transform t;
  // Debug::drawBasis(t, 0);
  // ImGui::gizmos(mCamera, transform, ImGuizmo::ROTATE);
  // mat44 view = mCamera.view(), proj = mCamera.projection();
  // mat44 trans = transform.localToWorld();
  // ImGuizmo::DrawCube((float*)&view, (float*)&proj, (float*)&trans);
  // mCamera.transfrom().localRotation() = vec3::zero;

  // antanuation
  mMoveSpeed = vec3::zero;
  mForce = vec3::zero;
  
  mAngularSpeed = vec3::zero;
  mAngularForce = vec3::zero;
}

void CameraController::speedScale(float scale) {
  mSpeedScale = scale;
}

void CameraController::addForce(const vec3& force) {

  // assume the mass of the camera is 1
  mForce += force;

}

void CameraController::addAngularForce(const vec2& force) {
  mAngularForce += force;
}

vec3 CameraController::speed() const {
  return mMoveSpeed * mSpeedScale;
}
