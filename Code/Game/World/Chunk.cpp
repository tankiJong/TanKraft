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

  index = (x & Chunk::kSizeMaskX)
        | ((y << Chunk::kSizeBitX) & Chunk::kSizeMaskY)
        | ((z << (Chunk::kSizeBitX + Chunk::kSizeBitY)) & Chunk::kSizeMaskZ);

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

Chunk::~Chunk() {
  EXPECTS(mMesh == nullptr);  
}

void Chunk::Iterator::step(eNeighbor dir) {
  if(self != nullptr) {
    *this = self->neighbor(dir);
  }
}

void Chunk::Iterator::step(const ChunkCoords& deltaCoords) {

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

Chunk::Iterator Chunk::Iterator::operator+(const ChunkCoords& deltaCoords) const {
  Iterator iter = *this;
  iter.step(deltaCoords);
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::next(const BlockCoords& deltaCoords) const {
  BlockIter iter = *this;
  iter.step(deltaCoords);
  return iter;
}

void Chunk::BlockIter::step(const BlockCoords& deltaCoords) {
  if(!chunk.valid()) return;

  BlockCoords current = blockCoords;

  // check whether it's over the z bound
  current.z += deltaCoords.z;
  // deltaCoords.z = 0;

  if(current.z >= kSizeZ || current.z < 0) {
    chunk = { nullptr };
    return;
  }

  ChunkCoords chunkShift;
  if(int end = current.x + deltaCoords.x; end < kSizeX && end >=0) {
    chunkShift.x = 0;
    current.x = end;
  } else {
    // wrapping around the chunks
    int expect = current.x + deltaCoords.x;
    auto re      = std::div(expect, kSizeX);
    current.x    = re.rem  + (expect < 0 ? kSizeX : 0);
    chunkShift.x = re.quot + (expect < 0 ? -1 : 0);
  }

  if(int end = current.y + deltaCoords.y; end < kSizeY && end >=0) {
    chunkShift.y = 0;
    current.y = end;
  } else {
    // wrapping around the chunks
    int expect = current.y + deltaCoords.y;
    auto re      = std::div(expect, kSizeY);
    current.y    = re.rem  + (expect < 0 ? kSizeY : 0);
    chunkShift.y = re.quot + (expect < 0 ? -1 : 0);
  }

  chunk.step(chunkShift);

  // update myself
  blockCoords = current;
  block = &chunk->block(blockCoords.toIndex());

}

bool Chunk::BlockIter::valid() const {
  return chunk.valid();
}

Chunk::BlockIter Chunk::BlockIter::operator+(const BlockCoords& deltaCoords) const {
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

Chunk::BlockIter::BlockIter(Chunk* c, BlockCoords crds)
: chunk(c), blockCoords(crds) {
  if(chunk.valid()) {
    block = &chunk->block(crds.toIndex());
  }
}

void Chunk::onInit() {
  // SAFE_DELETE(mMesh);
  // mMesher.clear();
  // mMesher.setWindingOrder(WIND_CLOCKWISE);
  // mMesher.begin(DRAW_TRIANGES);
  // mMesher.quad(mCoords.pivotPosition(), {1, 0, 0}, {0, 1, 0}, vec2{float(kSizeX), float(kSizeY)} * .2f);
  // mMesher.end();
  // mMesh = mMesher.createMesh<vertex_lit_t>();

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

  // try to save it to disk
}

void Chunk::onRegisterToWorld(World* world) {
  mOwner = world;
  
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

void Chunk::onUnregisterFromWorld() {
  if(mNeighbors[NEIGHBOR_POS_X] != nullptr) {
    mNeighbors[NEIGHBOR_POS_X]->setNeighbor(NEIGHBOR_NEG_X, nullptr);
  }
  if(mNeighbors[NEIGHBOR_NEG_X] != nullptr) {
    mNeighbors[NEIGHBOR_NEG_X]->setNeighbor(NEIGHBOR_POS_X, nullptr);
  }
  if(mNeighbors[NEIGHBOR_POS_Y] != nullptr) {
    mNeighbors[NEIGHBOR_POS_Y]->setNeighbor(NEIGHBOR_NEG_Y, nullptr);
  }
  if(mNeighbors[NEIGHBOR_NEG_Y] != nullptr) {
    mNeighbors[NEIGHBOR_NEG_Y]->setNeighbor(NEIGHBOR_POS_Y, nullptr);
  }

  mOwner = nullptr;
}

void Chunk::generateBlocks() {
  float noises[kSizeX][kSizeY];

  constexpr BlockIndex kWorldSeaLevel = 100;
  constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

  vec2 base = mCoords.pivotPosition().xy();
  for(uint i = 0; i < kSizeX; i++) {
    for(uint j = 0; j < kSizeY; j++) {
     vec2 worldPosition = vec2(i, j) + base;
      float noise = Compute2dPerlinNoise(worldPosition.x , worldPosition.y, 200, 3);
      noises[i][j] = float(kChangeRange) * noise + float(kWorldSeaLevel);
    }
  }

  BlockDef* air = BlockDef::get("air");
  BlockDef* dust = BlockDef::get("dust");
  BlockDef* stone = BlockDef::get("stone");
  BlockDef* grass = BlockDef::get("grass");

  BlockIndex m = 0;
  for(int k = 0; k < kSizeZ; k++) {
    for(int j = 0; j < kSizeY; j++) {
      for(int i = 0; i < kSizeX; i++) {

        BlockCoords coords{i, j, k};

        float currentZMax = noises[coords.x][coords.y];

        if(coords.z > currentZMax) {
          mBlocks[m].reset(*air);
        } else if(coords.z >= currentZMax - 1) {
          mBlocks[m].reset(*grass);
        } else if(coords.z >= currentZMax - 3) {
          mBlocks[m].reset(*dust);
        } else {
          mBlocks[m].reset(*stone);
        }

        m++;

      }
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

  auto addBlock = [&](const BlockIter& block1, const vec3& pivot1) {

    /*
     *     2 ----- 1
     *    /|      /|
     *   3 ----- 0 |
     *   | 6 ----|-5
     *   |/      |/             x  
     *   7 ----- 4         y___/
     */  
    std::array<vec3, 8> vertices1 = {
      vec3{ 0 + pivot1.x, 0 + pivot1.y, 1 + pivot1.z },
      vec3{ 1 + pivot1.x, 0 + pivot1.y, 1 + pivot1.z },
      vec3{ 1 + pivot1.x, 1 + pivot1.y, 1 + pivot1.z },
      vec3{ 0 + pivot1.x, 1 + pivot1.y, 1 + pivot1.z },

      vec3{ 0 + pivot1.x, 0 + pivot1.y, 0 + pivot1.z },     
      vec3{ 1 + pivot1.x, 0 + pivot1.y, 0 + pivot1.z },
      vec3{ 1 + pivot1.x, 1 + pivot1.y, 0 + pivot1.z },
      vec3{ 0 + pivot1.x, 1 + pivot1.y, 0 + pivot1.z },
    };


    const BlockDef& def1 = block1->type();
    
    if(block1->id() != 0) {
      // sides
      if(!block1.next({1, 0, 0})->opaque()) {
        mMesher.quad(vertices1[5], vertices1[6], vertices1[2], vertices1[1],
            def1.uvs(BlockDef::FACE_SIDE));
      }
      
      if(!block1.next({-1, 0, 0})->opaque()) {
        mMesher.quad(vertices1[7], vertices1[4], vertices1[0], vertices1[3],
            def1.uvs(BlockDef::FACE_SIDE));
      }
      
      if(!block1.next({0, -1, 0})->opaque()) {
        mMesher.quad(vertices1[4], vertices1[5], vertices1[1], vertices1[0],
            def1.uvs(BlockDef::FACE_SIDE));
      }

      // bottom
      if(!block1.next({0, 0, -1})->opaque()) {
        mMesher.quad(vertices1[4], vertices1[7], vertices1[6], vertices1[5],
            def1.uvs(BlockDef::FACE_BTM));
      }
      

      if(!block1.next({0, 1, 0})->opaque()) {
        mMesher.quad(vertices1[6], vertices1[7], vertices1[3], vertices1[2],
            def1.uvs(BlockDef::FACE_SIDE));
      }
      
      // top
      if(!block1.next({0, 0, 1})->opaque()) {
        mMesher.quad(vertices1[0], vertices1[1], vertices1[2], vertices1[3], 
            def1.uvs(BlockDef::FACE_TOP));
      }
    };

  };
  mMesher.reserve(kTotalBlockCount);
  mMesher.clear();
  mMesher.setWindingOrder(WIND_CLOCKWISE);
  mMesher.begin(DRAW_TRIANGES);


  constexpr BlockIndex kWorldSeaLevel = 100;
  constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

  for(int k = 0; k < kSizeZ - 1; k++) {
    for(int j = 0; j < kSizeY; j++) {
      for(int i = 0; i < kSizeX; i++) {

        BlockCoords coords1{i, j, k};
        vec3 worldPosition1 = vec3(coords1) + mCoords.pivotPosition();

        BlockIter iter1 = blockIter(coords1);
        addBlock(iter1, worldPosition1);
      }
    }
  }

  mMesher.end();

  mMesh = mMesher.createMesh<vertex_lit_t>();

  mIsDirty = false;

  return true;
}

Chunk::Iterator Chunk::iterator() {
  return { this };
}

