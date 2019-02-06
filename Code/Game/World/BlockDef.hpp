#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Math/Primitives/AABB2.hpp"

class BlockDef {
protected:
  std::string mName;

  enum eUV {
    UV_TOP,
    UV_SIDE,
    UV_BTM,
  };
  aabb2 uvs[3];
};
