#include "Entity.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Game/Gameplay/Player.hpp"

Entity::Entity() {
  mTransform.setRotationOrder(ROTATION_YZX);
}

void Entity::onRender() {
  vec3 position =  collision.center();
  if(!possessed) {
    Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
    Debug::drawSphere(
      position, 
      collision.radius, 10, 10, 
      true, 0, Rgba(10, 10, 0, 255));
    Debug::setDepth(Debug::DEBUG_DEPTH_ENABLE);
    Debug::drawSphere(
      position, 
      collision.radius, 10, 10, 
      true, 0, Rgba::yellow);
  }
}


void Entity::accelerate(const vec3& force) {
  mAcceleration += force * kAccelerationScale;
}
