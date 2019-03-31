#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/vec3.hpp"
#include "Engine/Math/Transform.hpp"
#include "Engine/Math/Primitives/aabb3.hpp"
#include "Engine/Math/Primitives/sphere.hpp"

// aabb3 which can only rotate along Z-axis
class CollisionBox {
public:
  CollisionBox() = default;
  CollisionBox(float sizex, float sizey, float sizez);

  Transform* target;
  vec3 dimension;
};

class CollisionSphere {
public:
  CollisionSphere() = default;
  CollisionSphere(Transform& transform, float radius)
    : target(&transform)
    , radius(radius) {};

  vec3 center() const;
  Transform* target;
  float radius;
};