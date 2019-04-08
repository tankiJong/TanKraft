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
  CollisionSphere(Transform& transform, float radius, const vec3& offset)
    : target(&transform)
    , radius(radius)
    , offset(offset) {};

  vec3 center() const;
  Transform* target = nullptr;
  float radius = 0;
  vec3 offset = vec3::zero;
};
