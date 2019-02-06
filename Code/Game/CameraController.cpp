#include "CameraController.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Debug/Draw.hpp"
#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"
#include "Engine/Gui/ImGui.hpp"

static const vec3 MAX_ACCELERATION{ 1.f };

static const vec2 MAX_ANGULAR_ACCELERATION{ 180.f };

void CameraController::onInput() {

  {
    if(Input::Get().isKeyDown('W')) {
      addForce({ mCamera.transfrom().right().xy(), 0 });
    }
    if(Input::Get().isKeyDown('S')) {
      addForce({ -mCamera.transfrom().right().xy(), 0 });
    }
  }

  {
    if (Input::Get().isKeyDown('D')) {
      addForce({ -mCamera.transfrom().up().xy(), 0 });
    }
    if (Input::Get().isKeyDown('A')) {
      addForce({ mCamera.transfrom().up().xy(), 0 });
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

  if (Input::Get().isKeyDown(MOUSE_MBUTTON)) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition();
    addForce(-mCamera.transfrom().right() * deltaMouse.x);
    addForce(mCamera.transfrom().up() * deltaMouse.y);
  }

  if(Input::Get().isKeyDown(MOUSE_RBUTTON)) {
    vec2 deltaMouse = Input::Get().mouseDeltaPosition(true);
    addAngularForce(deltaMouse * 360);
  }


}

void CameraController::onUpdate(float dt) {
  
  // shifting
  {
    vec3 acceleration = mForce;
    clamp(acceleration, -MAX_ACCELERATION, MAX_ACCELERATION);

    mMoveSpeed += acceleration * dt;
    mCamera.translate(mMoveSpeed * dt * speedScale());
  }

  // rotating
  static Transform transform;
  {
    vec2 angularAcce = mAngularForce;
    clamp(angularAcce, -MAX_ANGULAR_ACCELERATION, MAX_ANGULAR_ACCELERATION);

    mAngularSpeed += angularAcce * dt;
    Euler angle{ 0, mAngularSpeed.y, mAngularSpeed.x };
    mCamera.rotate(angle);
    transform.localRotate(angle);
  }
  ImGui::Begin("Test Euler");
  ImGui::SliderFloat3("Rotation", (float*)&transform.localRotation(), -360, 360);
  ImGui::End();
  // transform.localRotation() = mCamera.transfrom().localRotation();
  Debug::drawBasis(transform, 0);
  // ImGui::gizmos(mCamera, transform, ImGuizmo::ROTATE);
  // mat44 view = mCamera.view(), proj = mCamera.projection();
  // mat44 trans = transform.localToWorld();
  // ImGuizmo::DrawCube((float*)&view, (float*)&proj, (float*)&trans);
  // mCamera.transfrom().localRotation() = vec3::zero;

  // antanuation
  mMoveSpeed = mMoveSpeed * .9f;
  mForce *= .5f;
  
  mAngularSpeed = mAngularSpeed * .9f;
  mAngularForce *= .5f;
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

void CameraController::acceleration(const vec3& acc) {}

vec3 CameraController::speed() const {
  return mMoveSpeed * mSpeedScale;
}
