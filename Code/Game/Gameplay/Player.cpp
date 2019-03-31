#include "Player.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Game/Utils/Config.hpp"

Player::Player() {
  collision = CollisionSphere(mTransform, .4f);
}

void Player::onInput() {
  EXPECTS(mCamera != nullptr);
  {
    if(Input::Get().isKeyDown('W')) {
      accelerate({ mCamera->transform().right().xy().normalized(), 0 });
    }
    if(Input::Get().isKeyDown('S')) {
      accelerate({ -mCamera->transform().right().xy().normalized(), 0 });
    }
  }

  {
    if (Input::Get().isKeyDown('D')) {
      accelerate({ -mCamera->transform().up().xy().normalized(), 0 });
    }
    if (Input::Get().isKeyDown('A')) {
      accelerate({ mCamera->transform().up().xy().normalized(), 0 });
    }
  }

  {
    if (Input::Get().isKeyDown('E')) {
      accelerate({ 0,0, 1 });
    }
    if (Input::Get().isKeyDown('Q')) {
      accelerate({ 0,0, -1 });
    }
  }
}

void Player::onUpdate() {

  float dt = GetMainClock().frame.second;
  mSpeed += mAcceleration * dt;

  switch(mPhysicsMode) { 
    case PHY_NORMAL:
      mAcceleration = { 0, 0, -Config::kGravity };
    break;
    case PHY_FLY: 
      mAcceleration = { 0, 0, 0 };
    break;
    case PHY_GHOST: 
      mAcceleration = { 0, 0, 0 };
    break;
  }
  
  vec3 ds = mSpeed * dt;
  mSpeed *= .5f;
  mTransform.localTranslate(ds);
}
