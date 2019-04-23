#include "Player.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Game/Utils/Config.hpp"

Player::Player() {
  mCollisions[0] = CollisionSphere(mTransform, .3f, vec3::zero);
  mCollisionCounts = 1;
}

void Player::onInput() {
  vec3 willpower = vec3::zero;

  EXPECTS(mCamera != nullptr);
  {
    if(Input::Get().isKeyDown('W')) {
      willpower += { mCamera->transform().right().xy().normalized(), 0 };
    }
    if(Input::Get().isKeyDown('S')) {
      willpower += { -mCamera->transform().right().xy().normalized(), 0 };
    }
  }

  {
    if (Input::Get().isKeyDown('D')) {
      willpower += { -mCamera->transform().up().xy().normalized(), 0 };
    }
    if (Input::Get().isKeyDown('A')) {
      willpower += { mCamera->transform().up().xy().normalized(), 0 };
    }
  }

  {
    if(mPhysicsMode == PHY_NORMAL && Input::Get().isKeyJustDown(KEYBOARD_SPACE)) {
      mSpeed.z += 10.f;
    }
  }

  if(mPhysicsMode == PHY_GHOST) {
    if (Input::Get().isKeyDown('E')) {
      willpower += { 0, 0, 1.f };
    }
    if (Input::Get().isKeyDown('Q')) {
      willpower += { 0, 0, -1.f };
    }
  }

  addWillpower(willpower);
}

void Player::onUpdate() {
  Entity::onUpdate();
}

void Spectator::onInput() {
  Player::onInput();
  mSpeed = vec3::zero;

  mWillpower *= Input::Get().isKeyDown(KEYBOARD_SHIFT) ? 10.f : 1.f;
}
