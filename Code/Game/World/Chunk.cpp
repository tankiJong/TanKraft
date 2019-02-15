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
  return vec3{float(x*Chunk::kSizeX), float(y*Chunk::kSizeY), 0};
}

ChunkCoords ChunkCoords::fromWorld(vec3 position) {
  return { (int)floor(position.x) / Chunk::kSizeX, (int)floor(position.y) / Chunk::kSizeY};
}

void Chunk::onInit() {
  SAFE_DELETE(mMesh);
  mMesher.clear();
  mMesher.setWindingOrder(WIND_CLOCKWISE);
  mMesher.begin(DRAW_TRIANGES);
  mMesher.quad(mCoords.pivotPosition(), {1, 0, 0}, {0, 1, 0}, vec2{float(kSizeX), float(kSizeY)} * .2f);
  mMesher.end();
  mMesh = mMesher.createMesh<vertex_lit_t>();

  mIsDirty = true;

  // check 
  // if can load it from disk
  // if not, generate it
  generateBlocks();
}

void Chunk::onUpdate() {

}

void Chunk::onDestroy() {
  SAFE_DELETE(mMesh);

  // get rid of all connections to neighbors
  // try to save it to disk
}

void Chunk::generateBlocks() {
  for(int i = 0; i < (int)kTotalBlockCount; i++) {

    constexpr BlockIndex kWorldSeaLevel = 100;
    constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

    BlockCoords coords = BlockCoords::fromIndex((uint16_t)i);
    vec3 worldPosition = vec3(coords) + mCoords.pivotPosition();

    float noise = Compute2dPerlinNoise(worldPosition.x , worldPosition.y, 100, 1);

    float currentZMax = float(kChangeRange) * noise + float(kWorldSeaLevel);

    BlockDef* air = BlockDef::get("air");
    BlockDef* dust = BlockDef::get("dust");
    BlockDef* stone = BlockDef::get("stone");
    BlockDef* grass = BlockDef::get("grass");

    if(coords.z > currentZMax) {
      mBlocks[i].reset(*air);
    } else if(coords.z >= currentZMax - 1) {
      mBlocks[i].reset(*grass);
    } else if(coords.z >= currentZMax - 3) {
      mBlocks[i].reset(*dust);
    } else {
      mBlocks[i].reset(*stone);
    }
  }
}

void Chunk::reconstructMesh() {
  EXPECTS(mIsDirty);
  SAFE_DELETE(mMesh);

  // mMesher.clear();
  // mMesher.setWindingOrder(WIND_CLOCKWISE);
  // mMesher.begin(DRAW_TRIANGES);
  // mMesher.quad(mCoords.pivotPosition(), {1, 0, 0}, {0, 1, 0}, vec2{float(kSizeX), float(kSizeY)} * .8f);
  // mMesher.end();
  // mMesh = mMesher.createMesh<vertex_lit_t>();
  // mIsDirty = false;
  // return;

  auto addBlock = [&](Block& block, vec3 center) {
    vec3 dimension = vec3::one;
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

    const BlockDef* def = &block.type();

    if(block.id() == 0) return;

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
    vec3 worldPosition = vec3(coords) + mCoords.pivotPosition() + vec3{.5f};

    addBlock(mBlocks[i], worldPosition);

  }
  mMesher.end();

  mMesh = mMesher.createMesh<vertex_lit_t>();

  mIsDirty = false;
}
