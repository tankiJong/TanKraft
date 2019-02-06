#include "Chunk.hpp"

bool ChunckCoords::operator>(const ChunckCoords& rhs) const {
  return (x != rhs.x) ? (x > rhs.x)
    : ((y != rhs.y) ? (y > rhs.y) : false);
}

vec3 ChunckCoords::pivotPosition() {
  return vec3{};
}

void Chunk::onUpdate() {}

void Chunk::reconstructMesh() {
  auto addBlock = [&](const vec3& center, const vec3& dimension) {
    float dx = dimension.x * .5f, dy = dimension.y * .5f, dz = dimension.z * .5f;
    vec3 bottomCenter = center - vec3{0,0,1} * dz;

    std::array<vec3, 8> vertices = {
      bottomCenter + vec3{ -dx, -dy,  2*dz },
      bottomCenter + vec3{ dx,  -dy,  2*dz },
      bottomCenter + vec3{ dx,  dy, 2*dz },
      bottomCenter + vec3{ -dx, dy,  2*dz },

      bottomCenter + vec3{ -dx, -dz, 0 },
      bottomCenter + vec3{ dx,  -dz, 0 },
      bottomCenter + vec3{ dx,   dz, 0 },
      bottomCenter + vec3{ -dx,  dz, 0 }
    };

    mMesher.quad(vertices[0], vertices[1], vertices[2], vertices[3], 
        {0,.875}, {.125, .875}, {.125, 1}, {0, 1});
    mMesher.quad(vertices[4], vertices[7], vertices[6], vertices[5],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    mMesher.quad(vertices[4], vertices[5], vertices[1], vertices[0],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    mMesher.quad(vertices[5], vertices[6], vertices[2], vertices[1],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    mMesher.quad(vertices[6], vertices[7], vertices[3], vertices[2],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    mMesher.quad(vertices[7], vertices[4], vertices[0], vertices[3],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
  };
  
  mMesher.clear();
  mMesher.begin(DRAW_TRIANGES);
  for(int i = 0; i < (int)kTotalBlockCount; i++) {

    BlockCoords coords = BlockCoords::fromIndex((uint16_t)i);
    addBlock({ coords.centerPosition() }, vec3{ kBlockDim });

  }
  mMesher.end();

  mMesh = mMesher.createMesh<vertex_lit_t>();
}
