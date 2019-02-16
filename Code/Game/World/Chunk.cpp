#include "Chunk.hpp"
#include "Engine/Math/Noise/SmoothNoise.hpp"
#include "Game/World/BlockDef.hpp"
#include "Engine/Math/Primitives/AABB2.hpp"
#include <numeric>
#include "Game/World/World.hpp"

static constexpr int kDivNumMax = 32;
static constexpr int kDivDividerMax = 32;

struct div_tt {
  constexpr div_tt(int rem, int quot): rem(rem), quot(quot) {}
  constexpr div_tt(): rem(0), quot(0) {}
  int rem;
  int quot;
};
constexpr auto genDivTable() {
}

// [num][divider]
static constexpr auto gPositiveDivLookUp = []() constexpr {
  std::array<std::array<div_tt, kDivNumMax>, kDivDividerMax> arr;
  for(int i = 0; i < kDivNumMax; i++) {
    for(int j = 1; j < kDivDividerMax; j++) {
      arr[j][i] = div_tt(i % j, i / j);
    }
  }
  return arr;
}();

static constexpr auto gNegativeDivLookUp = []() constexpr {
  std::array<std::array<div_tt, kDivNumMax>, kDivDividerMax> arr;
  for(int i = 0; i < kDivNumMax; i++) {
    for(int j = 1; j < kDivDividerMax; j++) {
      arr[j][i] = div_tt((-i % j) + j, i / j - 1);
    }
  }
  return arr;
}();

inline div_tt quick_div(int num, int divider) {
  // EXPECTS(divider != 0);
  int absnum = abs(num);
  if(absnum < divider) {
    if(num >= 0) {
      return { num, 0};
    } else {
      return { num + divider, -1 };
    }
  }
  if(absnum < kDivNumMax && divider < kDivDividerMax) {
    if(num >= 0) {
      return gPositiveDivLookUp[divider][num];
    } else {
      return gNegativeDivLookUp[divider][-num];
    }
  } else {
    div_tt result;
    auto re      = std::div(num, divider);
    result.rem   = re.rem  + (num < 0 ? divider : 0);
    result.quot  = re.quot + (num < 0 ? -1 : 0);
    return result;
  }
} 
BlockIndex BlockCoords::toIndex() const {
  BlockIndex index = 0;
  EXPECTS(x >=0);
  EXPECTS(y >=0);
  EXPECTS(z >=0);

  index |= uint16_t(x) & Chunk::kSizeMaskX;
  index |= (uint16_t(y) << Chunk::kSizeBitX) & Chunk::kSizeMaskY;
  index |= (uint16_t(z) << (Chunk::kSizeBitX + Chunk::kSizeBitY)) & Chunk::kSizeMaskZ;

  return index;
}

inline BlockCoords BlockCoords::fromIndex(BlockIndex index) {
  return { Chunk::kSizeMaskX & index, 
          (Chunk::kSizeMaskY & index) >> Chunk::kSizeBitX,
          (Chunk::kSizeMaskZ & index) >> (Chunk::kSizeBitX + Chunk::kSizeBitY) };
}

BlockIndex BlockCoords::stepIndex(BlockIndex index, BlockCoords delta, ChunkCoords* outChunkShift) {
  BlockIndex ret = 0;

  uint newIndexX = Chunk::kSizeMaskX & index;
  uint deltaX = 0u | int32_t(delta.x);
  newIndexX += deltaX;

  ret |= newIndexX &  Chunk::kSizeMaskX;
  outChunkShift->x =  int(newIndexX) >> Chunk::kSizeBitX;

  uint newIndexY = Chunk::kSizeMaskY & index;
  uint deltaY = (0u | int32_t(delta.y)) << Chunk::kSizeBitX;
  newIndexY += deltaY;

  ret |= newIndexY & Chunk::kSizeMaskY;
  outChunkShift->y = int(newIndexY) >> (Chunk::kSizeBitX + Chunk::kSizeBitY);

  uint newIndexZ = Chunk::kSizeMaskZ & index;
  uint deltaZ = (0u | int32_t(delta.z)) << (Chunk::kSizeBitX + Chunk::kSizeBitY);
  newIndexZ += deltaZ;

  ret |= newIndexZ & Chunk::kSizeMaskZ;

  return ret;
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

void Chunk::Iterator::step(eNeighbor dir) {
  if(self != nullptr) {
    *this = self->neighbor(dir);
  }
}

void Chunk::Iterator::step(ChunkCoords deltaCoords) {

  if(self == nullptr) return;
  
  eNeighbor dirX = deltaCoords.x > 0 ? NEIGHBOR_POS_X : NEIGHBOR_NEG_X;
  eNeighbor dirY = deltaCoords.y > 0 ? NEIGHBOR_POS_Y : NEIGHBOR_NEG_Y;

  for(int i = 0; i < abs(deltaCoords.x); i++) {
    step(dirX);
  }

  for(int i = 0; i < abs(deltaCoords.y); i++) {
    step(dirY);
  }

}

Chunk::Iterator Chunk::Iterator::operator+(ChunkCoords deltaCoords) const {
  Iterator iter = *this;
  iter.step(deltaCoords);
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::next(BlockCoords deltaCoords) const {
  BlockIter iter = *this;
  iter.step(deltaCoords);
  return iter;
}

void Chunk::BlockIter::step(BlockCoords deltaCoords) {
  if(!chunk.valid()) return;

  BlockCoords current = BlockCoords::fromIndex(blockIndex);

  // check whether it's over the z bound
  current.z += deltaCoords.z;
  deltaCoords.z = 0;

  if(current.z >= kSizeZ || current.z < 0) {
    chunk = { nullptr };
    return;
  }

  // wrapping around the chunks
  {
    ChunkCoords chunkShift;
    {
      int expect = current.x + deltaCoords.x;
      auto re      = quick_div(expect, kSizeX);
      current.x    = re.rem  ;
      chunkShift.x = re.quot ;
    }
    {
      int expect = current.y + deltaCoords.y;
      auto re      = quick_div(expect, kSizeY);
      current.y    = re.rem  ;
      chunkShift.y = re.quot ;
    }

    chunk.step(chunkShift);
  }

  // update myself
  blockIndex = current.toIndex();

  block = &chunk->block(blockIndex);

}

bool Chunk::BlockIter::valid() const {
  return chunk.valid();
}

Chunk::BlockIter Chunk::BlockIter::operator+(BlockCoords deltaCoords) const {
  BlockIter iter = *this;
  iter.step(deltaCoords);
  return iter;
}

Block& Chunk::BlockIter::operator*() const {
  if(chunk.valid()) {
    return *block;
  } else {
    return Block::invalid;
  }
}

Block* Chunk::BlockIter::operator->() const {
  return block;
}

Chunk::BlockIter::BlockIter(Chunk* c, BlockIndex idx)
: chunk(c), blockIndex(idx) {
  if(chunk.valid()) {
    block = &chunk->block(blockIndex);
  }
}

void Chunk::onInit(World* world) {
  // SAFE_DELETE(mMesh);
  // mMesher.clear();
  // mMesher.setWindingOrder(WIND_CLOCKWISE);
  // mMesher.begin(DRAW_TRIANGES);
  // mMesher.quad(mCoords.pivotPosition(), {1, 0, 0}, {0, 1, 0}, vec2{float(kSizeX), float(kSizeY)} * .2f);
  // mMesher.end();
  // mMesh = mMesher.createMesh<vertex_lit_t>();

  mIsDirty = true;

  mOwner = world;

  // check 
  // if can load it from disk

  // if not, generate it
  generateBlocks();
  // setting up neighbors

  // +x
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{1, 0});
    setNeighbor(NEIGHBOR_POS_X, c);
    if(c != nullptr) c->setNeighbor(NEIGHBOR_NEG_X, this);
  }

  // -x
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{-1, 0});
    setNeighbor(NEIGHBOR_NEG_X, c);
    if(c != nullptr) c->setNeighbor(NEIGHBOR_POS_X, this);
  }

  // +y
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{0, 1});
    setNeighbor(NEIGHBOR_POS_Y, c);
    if(c != nullptr) c->setNeighbor(NEIGHBOR_NEG_Y, this);
  }

  // -y
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{0, -1});
    setNeighbor(NEIGHBOR_NEG_Y, c);
    if(c != nullptr) c->setNeighbor(NEIGHBOR_POS_Y, this);
  }
}

void Chunk::onUpdate() {

}

void Chunk::onDestroy() {
  SAFE_DELETE(mMesh);

  // get rid of all connections to neighbors
  // try to save it to disk
}

void Chunk::generateBlocks() {
  float noises[kSizeX][kSizeY];

  vec2 base = mCoords.pivotPosition().xy();
  for(uint i = 0; i < kSizeX; i++) {
    for(uint j = 0; j < kSizeY; j++) {
     vec2 worldPosition = vec2(i, j) + base;
      noises[i][j] = Compute2dPerlinNoise(worldPosition.x , worldPosition.y, 100, 1);
    }
  }

  BlockDef* air = BlockDef::get("air");
  BlockDef* dust = BlockDef::get("dust");
  BlockDef* stone = BlockDef::get("stone");
  BlockDef* grass = BlockDef::get("grass");

  for(int i = 0; i < (int)kTotalBlockCount; i++) {

    constexpr BlockIndex kWorldSeaLevel = 100;
    constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

    BlockCoords coords = BlockCoords::fromIndex((uint16_t)i);

    float noise = noises[coords.x][coords.y];

    float currentZMax = float(kChangeRange) * noise + float(kWorldSeaLevel);

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

bool Chunk::neighborsLoaded() const {
  return std::reduce(mNeighbors.begin(), mNeighbors.end(), true, [](bool before, Chunk* b) {
    return before && b != nullptr;
  });
}

bool Chunk::reconstructMesh() {
  EXPECTS(mIsDirty);

  if(!neighborsLoaded()) return false;
  SAFE_DELETE(mMesh);

  // mMesher.clear();
  // mMesher.setWindingOrder(WIND_CLOCKWISE);
  // mMesher.begin(DRAW_TRIANGES);
  // mMesher.quad(mCoords.pivotPosition(), {1, 0, 0}, {0, 1, 0}, vec2{float(kSizeX), float(kSizeY)} * .8f);
  // mMesher.end();
  // mMesh = mMesher.createMesh<vertex_lit_t>();
  // mIsDirty = false;
  // return true;

  auto addBlock = [&](BlockIter block, vec3 pivot) {

    /*
     *     2 ----- 1
     *    /|      /|
     *   3 ----- 0 |
     *   | 6 ----|-5
     *   |/      |/             x  
     *   7 ----- 4         y___/
     */  
    std::array<vec3, 8> vertices = {
      vec3{ 0 + pivot.x, 0 + pivot.y, 1 + pivot.z },
      vec3{ 1 + pivot.x, 0 + pivot.y, 1 + pivot.z },
      vec3{ 1 + pivot.x, 1 + pivot.y, 1 + pivot.z },
      vec3{ 0 + pivot.x, 1 + pivot.y, 1 + pivot.z },

      vec3{ 0 + pivot.x, 0 + pivot.y, 0 + pivot.z },     
      vec3{ 1 + pivot.x, 0 + pivot.y, 0 + pivot.z },
      vec3{ 1 + pivot.x, 1 + pivot.y, 0 + pivot.z },
      vec3{ 0 + pivot.x, 1 + pivot.y, 0 + pivot.z },
    };

    const BlockDef& def = block->type();

    
    if(block->id() == 0) return;



    // sides
    if(!block.next({1, 0, 0})->opaque()) {
      mMesher.quad(vertices[5], vertices[6], vertices[2], vertices[1],
          def.uvs(BlockDef::FACE_SIDE));
    }
    
    if(!block.next({-1, 0, 0})->opaque()) {
      mMesher.quad(vertices[7], vertices[4], vertices[0], vertices[3],
          def.uvs(BlockDef::FACE_SIDE));
    }
    
    if(!block.next({0, -1, 0})->opaque()) {
      mMesher.quad(vertices[4], vertices[5], vertices[1], vertices[0],
          def.uvs(BlockDef::FACE_SIDE));
    }

    // bottom
    if(!block.next({0, 0, -1})->opaque()) {
      mMesher.quad(vertices[4], vertices[7], vertices[6], vertices[5],
          def.uvs(BlockDef::FACE_BTM));
    }
    

    if(!block.next({0, 1, 0})->opaque()) {
      mMesher.quad(vertices[6], vertices[7], vertices[3], vertices[2],
          def.uvs(BlockDef::FACE_SIDE));
    }
    
    // top
    if(!block.next({0, 0, 1})->opaque()) {
      mMesher.quad(vertices[0], vertices[1], vertices[2], vertices[3], 
          def.uvs(BlockDef::FACE_TOP));
    }

  };
  mMesher.reserve(kTotalBlockCount);
  mMesher.clear();
  mMesher.setWindingOrder(WIND_CLOCKWISE);
  mMesher.begin(DRAW_TRIANGES);
  for(int i = 0; i < (int)kTotalBlockCount; i++) {

    constexpr BlockIndex kWorldSeaLevel = 100;
    constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

    BlockCoords coords = BlockCoords::fromIndex((uint16_t)i);
    vec3 worldPosition = vec3(coords) + mCoords.pivotPosition();

    BlockIter iter = blockIter(i);
    addBlock(iter, worldPosition);

  }
  mMesher.end();

  mMesh = mMesher.createMesh<vertex_lit_t>();

  mIsDirty = false;

  return true;
}

Chunk::Iterator Chunk::iterator() {
  return { this };
}

