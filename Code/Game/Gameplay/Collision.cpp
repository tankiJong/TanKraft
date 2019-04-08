#include "Collision.hpp"

vec3 CollisionSphere::center() const {
  return target->position() + offset;
}
