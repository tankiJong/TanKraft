#include "Chunk.hpp"
#include "Engine/Math/Noise/SmoothNoise.hpp"
#include "Game/World/BlockDef.hpp"
#include "Engine/Math/Primitives/AABB2.hpp"

inline BlockCoords BlockCoords::fromIndex(BlockIndex index) {
  return { Chunk::kSizeMaskX & index, 
          (Chunk::kSizeMaskY & index) >> Chunk::kSizeBitX,
          (Chunk::kSizeMaskZ & index) >> (Chunk::kSizeBitX + Chunk::kSizeBitY) };
}

bool ChunkCoords::operator>(const ChunkCoords& rhs) const {
  return (y != rhs.y) ? (y > rhs.y)
    : ((x != rhs.x) ? (x > rhs.x) : false);
}

vec3 ChunkCoords::pivotPosition() {
  return Chunk::kBlockDim * vec3{float(x) * Chunk::kChunkDimX, float(y) * Chunk::kChunkDimY, 0};
}

void Chunk::onInit() {
}

void Chunk::onUpdate() {
  if(mIsDirty) {
    reconstructMesh();
    mIsDirty = false;
  }
}

void Chunk::generateBlocks() {
  
}

void Chunk::reconstructMesh() {
  EXPECTS(mIsDirty);
  auto addBlock = [&](const vec3& center, const vec3& dimension, float zMax) {
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

    BlockDef* def;
    if(center.z >= zMax - 1) {
      def = BlockDef::name("grass");
    } else if(center.z >= zMax - 3) {
      def = BlockDef::name("dust");
    } else {
      def = BlockDef::name("stone");
    }


    // top
    mMesher.quad(vertices[0], vertices[1], vertices[2], vertices[3], 
        def->uvs(BlockDef::FACE_TOP));

    // bottom
    mMesher.quad(vertices[4], vertices[7], vertices[6], vertices[5],
        def->uvs(BlockDef::FACE_BTM));

    // sides
    mMesher.quad(vertices[4], vertices[5], vertices[1], vertices[0],
        def->uvs(BlockDef::FACE_SIDE));
    mMesher.quad(vertices[5], vertices[6], vertices[2], vertices[1],
        def->uvs(BlockDef::FACE_SIDE));
    mMesher.quad(vertices[6], vertices[7], vertices[3], vertices[2],
        def->uvs(BlockDef::FACE_SIDE));
    mMesher.quad(vertices[7], vertices[4], vertices[0], vertices[3],
        def->uvs(BlockDef::FACE_SIDE));
  };
  
  mMesher.clear();
  mMesher.setWindingOrder(WIND_CLOCKWISE);
  mMesher.begin(DRAW_TRIANGES);
  for(int i = 0; i < (int)kTotalBlockCount; i++) {

    constexpr BlockIndex kWorldSeaLevel = 100;
    constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

    BlockCoords coords = BlockCoords::fromIndex((uint16_t)i);
    vec3 worldPosition = vec3(coords) + mCoords.pivotPosition();

    float noise = Compute2dPerlinNoise(worldPosition.x , worldPosition.y, 100, 1);

    float currentZMax = float(kChangeRange) * noise + float(kWorldSeaLevel);

    if(coords.z > currentZMax) continue;

    addBlock(worldPosition, vec3{ kBlockDim }, currentZMax);

  }
  mMesher.end();

  mMesh = mMesher.createMesh<vertex_lit_t>();

}
