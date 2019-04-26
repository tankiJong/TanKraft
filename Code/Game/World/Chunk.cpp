#include "Chunk.hpp"
#include "Engine/Math/Noise/SmoothNoise.hpp"
#include "Game/World/BlockDef.hpp"
#include "Engine/Math/Primitives/AABB2.hpp"
#include <numeric>
#include "Game/World/World.hpp"
#include "Game/Utils/FileCache.hpp"
#include "Engine/Math/MathUtils.hpp"

static constexpr int kDivNumMax = 32;
static constexpr int kDivDividerMax = 32;

Chunk Chunk::sInvalidChunk = {};


struct div_tt {
  constexpr div_tt(int rem, int quot): rem(rem), quot(quot) {}
  constexpr div_tt(): rem(0), quot(0) {}
  int rem;
  int quot;
};
// constexpr auto genDivTable() {
// }
//
// // [num][divider]
// static constexpr auto gPositiveDivLookUp = []() constexpr {
//   std::array<std::array<div_tt, kDivNumMax>, kDivDividerMax> arr;
//   for(int i = 0; i < kDivNumMax; i++) {
//     for(int j = 1; j < kDivDividerMax; j++) {
//       arr[j][i] = div_tt(i % j, i / j);
//     }
//   }
//   return arr;
// }();
//
// static constexpr auto gNegativeDivLookUp = []() constexpr {
//   std::array<std::array<div_tt, kDivNumMax>, kDivDividerMax> arr;
//   for(int i = 0; i < kDivNumMax; i++) {
//     for(int j = 1; j < kDivDividerMax; j++) {
//       arr[j][i] = div_tt((-i % j) + j, i / j - 1);
//     }
//   }
//   return arr;
// }();

inline div_tt quick_div(int num, uint pow2) {
  div_tt result;

  int divider = 1 << pow2;
  // result.rem = ((num % divider) + divider) % divider;
  // result.quot = num / divider;

  {
    auto re = std::div(num, divider);
    result.rem = re.rem + (num < 0) * divider;
    result.quot = re.quot;
  }
  return result;
}

BlockCoords BlockCoords::fromWorld(const Chunk* chunk, const vec3& world) {
  vec3 local = world - chunk->bounds().mins;
  // GUARANTEE_RECOVERABLE(local.x >= 0, "");
  // GUARANTEE_RECOVERABLE(local.y >= 0, "");
  // GUARANTEE_RECOVERABLE(local.z >= 0, "");
  return { (int)floor(local.x), (int)floor(local.y), (int)floor(local.z) };
}

aabb3 BlockCoords::blockBounds(Chunk* chunk, BlockIndex index) {
  vec3 mins = vec3{fromIndex(index)} + chunk->bounds().mins;
  return { mins, mins + vec3::one };
}

vec3 BlockCoords::blockCenterPosition(Chunk* chunk, BlockIndex index) {
  vec3 mins = vec3{fromIndex(index)} + chunk->bounds().mins;
  return mins + vec3(.5f);
}

bool ChunkCoords::operator>(const ChunkCoords& rhs) const {
  return (y != rhs.y) ? (y > rhs.y)
    : ((x != rhs.x) ? (x > rhs.x) : false);
}

vec3 ChunkCoords::pivotPosition() {
  return vec3{float(x*Chunk::kSizeX), float(y*Chunk::kSizeY), 0};
}

ChunkCoords ChunkCoords::fromWorld(vec3 position) {
  return { (int)floor(position.x / Chunk::kSizeX) , (int)floor(position.y / Chunk::kSizeY) };
}

Chunk::Chunk(ChunkCoords coords): mCoords(std::move(coords)) {
  mBounds = aabb3(coords.pivotPosition(), 
                  coords.pivotPosition() + vec3{(float)kSizeX, (float)kSizeY, (float)kSizeZ});
}

Chunk::~Chunk() {
  EXPECTS(mMesh == nullptr);  
}

void Chunk::Iterator::step(eNeighbor dir) {
  *this = self->neighbor(dir);
}

void Chunk::Iterator::step(const ChunkCoords& deltaCoords) {

  if(!valid()) return;
  
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

Chunk::BlockIter Chunk::BlockIter::nextNegX() const {
  BlockIter iter = *this;
  iter.stepNegX();
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::nextPosX() const {
  BlockIter iter = *this;
  iter.stepPosX();
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::nextNegY() const {
  BlockIter iter = *this;
  iter.stepNegY();
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::nextPosY() const {
  BlockIter iter = *this;
  iter.stepPosY();
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::nextNegZ() const {
  BlockIter iter = *this;
  iter.stepNegZ();
  return iter;
}

Chunk::BlockIter Chunk::BlockIter::nextPosZ() const {
  BlockIter iter = *this;
  iter.stepPosZ();
  return iter;
}

void Chunk::BlockIter::step(const BlockCoords& deltaCoords) {
  if(!chunk.valid()) return;

  BlockCoords current = BlockCoords::fromIndex(blockIndex);

  // check whether it's over the z bound
  current.z += deltaCoords.z;
  // deltaCoords.z = 0;

  if(current.z >= kSizeZ || current.z < 0) {
    chunk = { sInvalidChunk };
    return;
  }

  ChunkCoords chunkShift;
  // if(int end = current.x + deltaCoords.x; end < kSizeX && end >=0) {
  //   chunkShift.x = 0;
  //   current.x = end;
  {// } else {
    // wrapping around the chunks
    int expect = current.x + deltaCoords.x;
    auto re      = quick_div(expect, kSizeBitX);
    current.x    = re.rem  ;
    chunkShift.x = re.quot - (expect < 0);
  }
  //
  // if(int end = current.y + deltaCoords.y; end < kSizeY && end >=0) {
  //   chunkShift.y = 0;
  //   current.y = end;
  {// } else {
    // wrapping around the chunks
    int expect = current.y + deltaCoords.y;
    auto re      = quick_div(expect, kSizeBitY);
    current.y    = re.rem  ;
    chunkShift.y = re.quot - (expect < 0);
  }

  chunk.step(chunkShift);

  // update myself
  blockIndex = current.toIndex();

}

void Chunk::BlockIter::stepNegX() {
  if((blockIndex & kSizeMaskX) == 0) {
    blockIndex |= kSizeMaskX;
    chunk.step(NEIGHBOR_NEG_X);
  } else {
    blockIndex -= 1;
  }
}

void Chunk::BlockIter::stepPosX() {
  if((blockIndex & kSizeMaskX) == kSizeMaskX) {
    blockIndex &= ~kSizeMaskX;
    chunk.step(NEIGHBOR_POS_X);
  } else {
    blockIndex += 1;
  }
}

void Chunk::BlockIter::stepNegY() {
  if((blockIndex & kSizeMaskY) == 0) {
    blockIndex |= kSizeMaskY;
    chunk.step(NEIGHBOR_NEG_Y);
  } else {
    blockIndex -= kSizeX;
  }
}

void Chunk::BlockIter::stepPosY() {
  if((blockIndex & kSizeMaskY) == kSizeMaskY) {
    blockIndex &= ~kSizeMaskY;
    chunk.step(NEIGHBOR_POS_Y);
  } else {
    blockIndex += kSizeX;
  }
}

void Chunk::BlockIter::stepPosZ() {
  if((blockIndex & kSizeMaskZ) == kSizeMaskZ) {
    blockIndex &= ~kSizeMaskZ;
    chunk = invalidIter();
  } else {
    blockIndex += kSizeX * kSizeY;
  }
}

void Chunk::BlockIter::stepNegZ() {
  if((blockIndex & kSizeMaskZ) == 0) {
    blockIndex |= kSizeMaskZ;
    chunk = invalidIter();
  } else {
    blockIndex -= kSizeX * kSizeY;
  }
}

bool Chunk::BlockIter::valid() const {
  return chunk.valid();
}

void Chunk::BlockIter::reset(BlockDef& def) {

  Block* b = block();
  if(b == nullptr) return;

  b->resetFromDef(def);

  chunk->setDirty();
  if((blockIndex & kSizeMaskX) == 0) {
    auto iter = chunk->neighbor(NEIGHBOR_NEG_X);
    iter->setDirty();
  }
  if((blockIndex & kSizeMaskX) == kSizeMaskX) {
    auto iter = chunk->neighbor(NEIGHBOR_POS_X);
    iter->setDirty();
  }

  if((blockIndex & kSizeMaskY) == 0) {
    auto iter = chunk->neighbor(NEIGHBOR_NEG_Y);
    iter->setDirty();
  }
  if((blockIndex & kSizeMaskY) == kSizeMaskY) {
    auto iter = chunk->neighbor(NEIGHBOR_POS_Y);
    iter->setDirty();
  }
}

void Chunk::BlockIter::dirtyLight() {
  chunk->markBlockLightDirty(*this);
}

Chunk::BlockIter Chunk::BlockIter::operator+(const BlockCoords& deltaCoords) const {
  BlockIter iter = *this;
  iter.step(deltaCoords);
  return iter;
}

Block& Chunk::BlockIter::operator*() const {
  if(chunk.valid()) {
    return chunk->block(blockIndex);
  } else {
    return Block::kInvalid;
  }
}

Block* Chunk::BlockIter::operator->() const {
  return block();
}

Block* Chunk::BlockIter::block() const {
  if(chunk.valid()) {
    return &chunk->block(blockIndex);
  } else {
    return &Block::kInvalid;
  } 
}

Chunk::BlockIter::BlockIter(Chunk& c, BlockCoords crds)
: chunk(c), blockIndex(crds.toIndex()) {
}

Chunk::BlockIter::BlockIter(Chunk& c, BlockIndex index)
: chunk(c), blockIndex(index) {
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
  FileCache& cache = FileCache::get();
  if(!cache.load(*this)) {
    generateBlocks();
  }
  // if not, generate it

}

S<Job::Counter> Chunk::initAsync() {
  mState = CHUNK_STATE_LOADING;
  FileCache& cache = FileCache::get();

  S<Job::Counter> dummyJob = Job::create({[] {
  }}, Job::CAT_GENERIC);

  S<Job::Counter> loadJob = Job::create({[this, dummyJob] {
    FileCache& cache = FileCache::get();
    bool result = cache.load(*this);
    if(!result) {
      S<Job::Counter> counter = generateBlockAsync();
      Job::chain(counter, dummyJob);
      Job::dispatch(counter);
    } else {
      mState = CHUNK_STATE_LOADED_NO_MESH;
    }
  }}, Job::CAT_IO);

  Job::chain(loadJob, dummyJob);
  Job::dispatch(loadJob);
  return dummyJob;
}

S<Job::Counter> Chunk::generateBlockAsync() {
  mIsDirty = true;
  Job::Decl decl(this, &Chunk::generateBlocks);
  S<Job::Counter> generateBlockJob = Job::create(decl, Job::CAT_GENERIC);
  return generateBlockJob;
}

void Chunk::afterRegisterToWorld() {
  initLights();
}

void Chunk::onUpdate() {
}

void Chunk::onDestroy() {
  SAFE_DELETE(mMesh);

  if(mSavePending) {
    FileCache::get().save(*this);
  }
  // try to save it to disk
}

void Chunk::onRegisterToWorld(World* world) {
  mOwner = world;
  
  // +x
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{1, 0});
    setNeighbor(NEIGHBOR_POS_X, *c);
    if(c->valid()) c->setNeighbor(NEIGHBOR_NEG_X, *this);
  }

  // -x
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{-1, 0});
    setNeighbor(NEIGHBOR_NEG_X, *c);
    if(c->valid()) c->setNeighbor(NEIGHBOR_POS_X, *this);
  }

  // +y
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{0, 1});
    setNeighbor(NEIGHBOR_POS_Y, *c);
    if(c->valid()) c->setNeighbor(NEIGHBOR_NEG_Y, *this);
  }

  // -y
  {
    Chunk* c = mOwner->findChunk(mCoords + ChunkCoords{0, -1});
    setNeighbor(NEIGHBOR_NEG_Y, *c);
    if(c->valid()) c->setNeighbor(NEIGHBOR_POS_Y, *this);
  }
}

void Chunk::onUnregisterFromWorld() {
  if(mNeighbors[NEIGHBOR_POS_X]->valid()) {
    mNeighbors[NEIGHBOR_POS_X]->setNeighbor(NEIGHBOR_NEG_X, sInvalidChunk);
  }
  if(mNeighbors[NEIGHBOR_NEG_X]->valid()) {
    mNeighbors[NEIGHBOR_NEG_X]->setNeighbor(NEIGHBOR_POS_X, sInvalidChunk);
  }
  if(mNeighbors[NEIGHBOR_POS_Y]->valid()) {
    mNeighbors[NEIGHBOR_POS_Y]->setNeighbor(NEIGHBOR_NEG_Y, sInvalidChunk);
  }
  if(mNeighbors[NEIGHBOR_NEG_Y]->valid()) {
    mNeighbors[NEIGHBOR_NEG_Y]->setNeighbor(NEIGHBOR_POS_Y, sInvalidChunk);
  }

  mOwner = nullptr;
}

struct chunk_header_t {
  uint8_t cc[4]     = {'S', 'M', 'C', 'D'};
  uint8_t version   = 1;
  uint8_t chunkBitX = Chunk::kSizeBitX;
  uint8_t chunkBitY = Chunk::kSizeBitY;
  uint8_t chunkBitZ = Chunk::kSizeBitZ;
  uint8_t reserved1 = 0;
  uint8_t reserved2 = 0;
  uint8_t reserved3 = 0;
  uint8_t format    = 'R';

  bool operator==(const chunk_header_t& rhs) const {
    return memcmp(this, &rhs, sizeof(chunk_header_t)) == 0;
  };
};
struct entry_t {
  uint8_t type = 0;
  uint8_t count = 0;
};

size_t Chunk::serialize(byte_t* data, size_t maxWrite) const {

  size_t totalWrite = 0;
  // write header
  chunk_header_t* header = (chunk_header_t*)data;
  *header = chunk_header_t();

  entry_t* e = (entry_t*)(header+1);
  totalWrite += sizeof(chunk_header_t);
  {
    e->type = block(0).id();
    e->count = 1;
  }
  uint blockCount = 0;
  for(uint i = 1; i < kTotalBlockCount; i++) {
    const Block& b = block(i);
    if(e->type != b.id() || e->count == 255) {
      blockCount+=e->count;
      e++;
      totalWrite += sizeof(entry_t); 
      
      e->type = b.id();
      ENSURES(e->type <= 4);
      e->count = 1;
      ENSURES(totalWrite <= maxWrite);
    } else {
      e->count++;
    }
  }

  if(e->count != 0) {
    totalWrite += sizeof(entry_t);
    blockCount+=e->count;
  }
  ENSURES(totalWrite <= maxWrite && ((totalWrite & 1) == 0));
  ENSURES(blockCount == kTotalBlockCount);
  return totalWrite;
}

void Chunk::deserialize(byte_t* data, size_t maxRead) {

  size_t totalRead = sizeof(chunk_header_t);
  {
    chunk_header_t* header = (chunk_header_t*)data;
    ENSURES(*header == chunk_header_t());
  }
  ENSURES(totalRead < maxRead);
  entry_t* entry = (entry_t*)(data + sizeof(chunk_header_t));

  BlockIndex index = 0;
  while(totalRead < maxRead) {
    BlockDef* def = BlockDef::get(entry->type);

    for(BlockIndex i = 0; i < entry->count; i++) {
      BlockIndex bi = i + index;
      resetBlock(bi, *def);
    }

    index += entry->count;
    totalRead += sizeof(entry_t);
    entry++;
  }

  // should be wrap around;
  ENSURES(index == 0);
}

void Chunk::resetBlock(BlockIndex index, BlockDef& def) {
  BlockIter iter(*this, index);
  iter.reset(def);
}

void Chunk::addBlock(const BlockIter& block, const vec3& pivot) {

    /*
     *     2 ----- 1
     *    /|      /|
     *   3 ----- 0 |
     *   | 6 ----|-5
     *   |/      |/             x  
     *   7 ----- 4         y___/
     */  
    vec3 vertices[8] = {
      vec3{ 0 + pivot.x, 0 + pivot.y, 1 + pivot.z },
      vec3{ 1 + pivot.x, 0 + pivot.y, 1 + pivot.z },
      vec3{ 1 + pivot.x, 1 + pivot.y, 1 + pivot.z },
      vec3{ 0 + pivot.x, 1 + pivot.y, 1 + pivot.z },

      vec3{ 0 + pivot.x, 0 + pivot.y, 0 + pivot.z },     
      vec3{ 1 + pivot.x, 0 + pivot.y, 0 + pivot.z },
      vec3{ 1 + pivot.x, 1 + pivot.y, 0 + pivot.z },
      vec3{ 0 + pivot.x, 1 + pivot.y, 0 + pivot.z },
    };

    struct Face {
      uint v[4];
    };

    static constexpr Face faces[6] = {
      {5, 6, 2, 1},
      {7, 4, 0, 3},
      {4, 5, 1, 0},
      {6, 7, 3, 2},
      {4, 7, 6, 5},
      {0, 1, 2, 3}
    };

    static const vec3 normals[6] = {
      {1, 0, 0},
      {-1, 0, 0},
      {0, -1, 0},
      {0, 1, 0},
      {0, 0, -1},
      {0, 0, 1}
    };

    static const vec3 tangents[6] = {
      {0, 1, 0},
      {0, 1, 0},
      {1, 0, 0},
      {1, 0, 0},
      {1, 0, 0},
      {1, 0, 0}
    };

    static constexpr BlockDef::eFace uvs[6] = {
      BlockDef::FACE_SIDE,
      BlockDef::FACE_SIDE,
      BlockDef::FACE_SIDE,
      BlockDef::FACE_SIDE,
      BlockDef::FACE_BTM,
      BlockDef::FACE_TOP
    };

    BlockIter neighbors[6] = {
      block.nextPosX(),
      block.nextNegX(),
      block.nextNegY(),
      block.nextPosY(),
      block.nextNegZ(),
      block.nextPosZ()
    };


    const BlockDef& def = block->type();
    if(block->id() != 0) {
      for(uint i = 0; i < 6; i++) {
        BlockIter neighbor = neighbors[i];
        if(!neighbor->opaque()) {
          mMesher.normal(normals[i]);
          mMesher.tangent(tangents[i]);
          aabb2 uv = def.uvs(uvs[i]);

          Face face = faces[i];

          Rgba color(neighbor->indoorLight() * 16, neighbor->outdoorLight() * 16, 0);
          // if(neighbor->indoorLight()) {
          //   DEBUGBREAK;
          // }
          mMesher.color(color);

          mMesher.uv(uv.mins)
                 .vertex3f(vertices[face.v[0]]);
          mMesher.uv({uv.maxs.x, uv.mins.y})
                 .vertex3f(vertices[face.v[1]]);
          mMesher.uv(uv.maxs)
                 .vertex3f(vertices[face.v[2]]);
          mMesher.uv({uv.mins.x, uv.maxs.y})
                 .vertex3f(vertices[face.v[3]]);
          mMesher.quad();
        }
      }
    }
    
  }

void Chunk::markBlockLightDirty(const BlockIter& block) {
  if(block->lightDirty()) return;
  mOwner->submitDirtyBlock(block);
  block->setLightDirty();
};

void Chunk::generateBlocks() {
  float noises[kSizeX][kSizeY];

  constexpr BlockIndex kWorldSeaLevel = 100;
  constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 3;

  vec2 base = mCoords.pivotPosition().xy();
  for(uint i = 0; i < kSizeX; i++) {
    for(uint j = 0; j < kSizeY; j++) {
     vec2 worldPosition = vec2((float)i, (float)j) + base;
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
          resetBlock(m, *air);
        } else if(coords.z >= currentZMax - 1) {
          resetBlock(m, *grass);
        } else if(coords.z >= currentZMax - 3) {
         resetBlock(m, *dust);
        } else {
         resetBlock(m, *stone);
        }

        m++;

      }
    }
  }

  mState = CHUNK_STATE_LOADED_NO_MESH;
}

void Chunk::initLights() {
  
  // populate outdoor lighting
  for(BlockIndex y = 0; y < kSizeY; y++) {
    for(BlockIndex x = 0; x < kSizeX; x++) {
      BlockIndex index = BlockCoords::toIndex(x, y, kSizeZ - 1);
      BlockIter iter = blockIter(index);
      while(!iter->opaque() && iter.valid()) {
        iter->setSky();
        iter.stepNegZ();
      }
    }
  }

  for(BlockIndex y = 0; y < kSizeY; y++) {
    for(BlockIndex x = 0; x < kSizeX; x++) {
      BlockIndex index = BlockCoords::toIndex(x, y, kSizeZ - 1);
      BlockIter iter = blockIter(index);
      while(!iter->opaque() && iter.valid()) {
        EXPECTS(iter->exposedToSky());

        BlockIter neighbors[4] = {
          iter.nextNegX(),
          iter.nextPosX(),
          iter.nextNegY(),
          iter.nextPosY(),
        };

        for(BlockIter& neighbor: neighbors) {
          if(neighbor->exposedToSky()) continue;
          if(!neighbor.valid()) continue;
          if(neighbor->opaque()) continue;
          markBlockLightDirty(neighbor);
        }

        iter.stepNegZ();
      }
    }
  }

  // return;


  /*
   *         +x
   *        ┌──┐       x
   *      +y│  │-y   y─┘
   *        └──┘
   *         -x
   */

  // +x
  if(neighbor(NEIGHBOR_POS_X).valid()) {
    uint16_t x = kSizeMaskX;
    for(uint16_t z = 0; z < kSizeZ; z++) {
      for(uint16_t y = 0; y < kSizeY; y++) {
        BlockIndex index = BlockCoords::toIndex(x, y, z);
        BlockIter iter = blockIter(index);
        if(!iter->opaque()) {
          markBlockLightDirty(iter);
        }
      }
    }
  }
  
  // -x
  if(neighbor(NEIGHBOR_NEG_X).valid()) {
    uint16_t x = 0;
    for(uint16_t z = 0; z < kSizeZ; z++) {
      for(uint16_t y = 0; y < kSizeY; y++) {
        BlockIndex index = BlockCoords::toIndex(x, y, z);
        BlockIter iter = blockIter(index);
        if(!iter->opaque()) {
          markBlockLightDirty(iter);
        }
      }
    }
  }
  
  // +y
  if(neighbor(NEIGHBOR_POS_Y).valid()) {
    uint16_t y = kSizeMaskY;
    for(uint16_t z = 0; z < kSizeZ; z++) {
      for(uint16_t x = 0; x < kSizeX; x++) {
        BlockIndex index = BlockCoords::toIndex(x, y, z);
        BlockIter iter = blockIter(index);
        if(!iter->opaque()) {
          markBlockLightDirty(iter);
        }
      }
    }
  }
  
  // -y
  if(neighbor(NEIGHBOR_NEG_Y).valid()) {
    uint16_t y = 0;
    for(uint16_t z = 0; z < kSizeZ; z++) {
      for(uint16_t x = 0; x < kSizeX; x++) {
        BlockIndex index = BlockCoords::toIndex(x, y, z);
        BlockIter iter = blockIter(index);
        if(!iter->opaque()) {
          markBlockLightDirty(iter);
        }
      }
    }
  }

  for(uint i = 0; i < kTotalBlockCount; i++) {
    Block& b = mBlocks[i];
    // light source
    if(b.type().emissive() > 0) {
      markBlockLightDirty({ *this, (BlockIndex)i});
    } 
  }

}

bool Chunk::neighborsLoaded() const {
  return std::reduce(mNeighbors.begin(), mNeighbors.end(), true, [](bool before, Chunk* b) {
    return before && b->valid();
  });
}

void Chunk::rebuildGpuMetaData() {
  mChunkGPUData = Texture3::create(kSizeX, kSizeY, kSizeZ, TEXTURE_FORMAT_R32_UINT, 
	                RHIResource::BindingFlag::ShaderResource | RHIResource::BindingFlag::UnorderedAccess, mBlocks.data());
  setName(*mChunkGPUData, make_wstring(Stringf("C(%d, %d)", mCoords.x, mCoords.y)).c_str());
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

  
  mMesher.reserve(kSizeX * kSizeY * 3);
  mMesher.clear();
  mMesher.setWindingOrder(WIND_CLOCKWISE);
  mMesher.begin(DRAW_TRIANGES);


  constexpr BlockIndex kWorldSeaLevel = 100;
  constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

  for(int k = 0; k < kSizeZ; k++) {
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

  rebuildGpuMetaData();

  mIsDirty = false;

  return true;
}

S<Job::Counter> Chunk::reconstructMeshAsync() {
  if(!neighborsLoaded()) return nullptr;
  mState = CHUNK_STATE_MESH_CONSTRUCTING;
  Job::Decl constructCPUMesh([this] {
    EXPECTS(mIsDirty);
    SAFE_DELETE(mMesh);
    
    mMesher.reserve(kSizeX * kSizeY * 3);
    mMesher.clear();
    mMesher.setWindingOrder(WIND_CLOCKWISE);
    mMesher.begin(DRAW_TRIANGES);


    constexpr BlockIndex kWorldSeaLevel = 100;
    constexpr int kChangeRange = (int(kSizeZ) - int(kWorldSeaLevel)) / 2;

    for(int k = 0; k < kSizeZ; k++) {
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

    S<Job::Counter> gpuMeshJob = Job::create({[this] {
      mMesh = mMesher.createMesh<vertex_lit_t>();
      rebuildGpuMetaData();
      mState = CHUNK_STATE_READY;
      mIsDirty = false;
    }}, Job::CAT_MAIN_THREAD);
    Job::dispatch(gpuMeshJob);
  });
  S<Job::Counter> cpuMeshJob = Job::create(constructCPUMesh, Job::CAT_GENERIC_SLOW);

  Job::dispatch(cpuMeshJob);
  return cpuMeshJob;
}

Chunk::Iterator Chunk::iterator() {
  return { *this };
}

Chunk::BlockIter Chunk::blockIter(const vec3& world) {

  return blockIter(BlockCoords::fromWorld(this, world));
}

