#include "Entity.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Game/Gameplay/Player.hpp"
#include "Game/Utils/Config.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Math/MathUtils.hpp"

Entity::Entity() {
  mTransform.setRotationOrder(ROTATION_YZX);
}

void Entity::onUpdate() {
  
  vec3 acce = mWillpower;

  if(mPhysicsMode == PHY_NORMAL) {
    acce.z -= Config::kGravity;
  }

  float dt = (float)GetMainClock().frame.second;
  mSpeed += mSpeedScale * acce * dt;

  float len = mSpeed.xy().magnitude();
  if(!equal(len, 0)) {
    len = clamp(len, 0.f, kMaxSpeed);
    mSpeed = mSpeedScale * vec3{ mSpeed.xy().normalized() * len, mSpeed.z };
  }

  vec3 ds = mSpeed * dt;
  mSpeed *= .9f;
  mTransform.localTranslate(ds);

  mWillpower = vec3::zero;
}

void Entity::onRender() {
  auto cs = collisions();
  for(CollisionSphere& collision: cs) {
    vec3 position =  collision.center();
    // if(!possessed) {
    {  
      vec3 worldStart = mTransform.transform(vec3::zero);
      Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
      Debug::drawSphere(
        position, 
        collision.radius, 10, 10, 
        true, 0, Rgba(0, 10, 10, 255));
      Debug::drawLine(worldStart, worldStart + mSpeed, 1, 0, Rgba(10, 10, 0, 255), Rgba::yellow);
      
      Debug::setDepth(Debug::DEBUG_DEPTH_ENABLE);
      Debug::drawSphere(
        position, 
        collision.radius, 10, 10, 
        true, 0, Rgba::cyan);
      Debug::drawLine(worldStart, worldStart + mSpeed, 1, 0, Rgba::yellow, Rgba::yellow);
    }
  }
}

void Entity::addWillpower(const vec3& power) {
  mWillpower += power * kAccelerationScale;
}
