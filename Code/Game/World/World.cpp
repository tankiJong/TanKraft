#include "World.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"

#include "Game/VoxelRenderer/VoxelRenderer.hpp"

#include "Game/World/Chunk.hpp"
#include "Game/Utils/Config.hpp"
#include "imgui/imgui_internal.h"
#include "Engine/Input/Input.hpp"
#include "Engine/Math/Primitives/ray3.hpp"
#include "Engine/Physics/contact3.hpp"
#include "Engine/Debug/Log.hpp"

void World::onInit() {

  if(sChunkActivationVisitingPattern.empty()) reconstructChunkVisitingPattern();

  mCamera.setCoordinateTransform(gGameCoordsTransform);
  // mCamera.transfrom().setCoordinateTransform(gGameCoordsTransform);
  mCamera.transform().setRotationOrder(ROTATION_YZX);
  mCamera.transform().localPosition() = vec3{-10,1,120};
  mCamera.transform().localRotation() = vec3{0,0,0};

  // TODO: look at is buggy
  // mCamera.lookAt({ -3, -3, 3 }, { 0, 0, 0 }, {0, 0, 1});
  mCamera.setProjectionPrespective(70, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, Config::kMaxActivateDistance);
 
  mCameraController.speedScale(10);

  Debug::setCamera(&mCamera);
  Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
  Debug::drawBasis(Transform());

  
}

void World::onInput() {

  mCameraController.onInput();

  {
    mCameraController.speedScale((Input::Get().isKeyDown(KEYBOARD_SHIFT) ? 100.f : 10.f));

    float scale = mCameraController.speedScale();
    vec3 camPosition = mCamera.transform().position();
    vec3 camRotation = mCamera.transform().localRotation();
    ImGui::Begin("Control");
    ImGui::Checkbox("Enable raycast", &mEnableRaycast);
    ImGui::SliderFloat("Camera speed", &scale, 1, 500, "%0.f", 10);
    ImGui::SliderFloat3("Camera Position", (float*)&camPosition, 0, 10);
    ImGui::SliderFloat3("Camera Rotation", (float*)&mCamera.transform().localRotation(), -360, 360);
    ImGui::End();
    mCameraController.speedScale(scale);
  }

  if(Input::Get().isKeyJustDown('R')) {
    mEnableRaycast = !mEnableRaycast;
  }

  if(Input::Get().isKeyJustDown(MOUSE_LBUTTON)) {
    if(mPlayerRaycast.impacted()) {
      BlockDef* air = BlockDef::get(0);
      mPlayerRaycast.contact.block.reset(*air);
      mPlayerRaycast.contact.block.dirtyLight();
      mPlayerRaycast.contact.block.chunk->markSavePending();
    }
  }

  if(Input::Get().isKeyJustDown(MOUSE_RBUTTON)) {
    if(mPlayerRaycast.impacted() && mPlayerRaycast.contact.distance > 0) {

      BlockDef* light = nullptr;

      if(Input::Get().isKeyDown(KEYBOARD_CONTROL)) {
        light = BlockDef::get("light");
      } else {
        light = BlockDef::get("stone");
      }
      auto iter = mPlayerRaycast.contact.block.next(mPlayerRaycast.contact.normal);
      iter.reset(*light);
      iter.dirtyLight();
      iter.chunk->markSavePending();
    }
  }

  if(Input::Get().isKeyJustDown('U')) {
    std::vector<ChunkCoords> chunks;
    chunks.reserve(mActiveChunks.size());

    for(const auto& [coords, chunk]: mActiveChunks) {
      if(chunk->valid()) {
        chunks.push_back(coords);
      }
    }

    for(ChunkCoords& c: chunks) {
      deactivateChunk(c);
    }
  }
}

void World::onUpdate() {
  float dt = (float)GetMainClock().frame.second;
  mCameraController.onUpdate(dt);

  updateChunks();
  manageChunks();

  static bool debug_light = false;

  if(Input::Get().isKeyJustDown(KEYBOARD_DOWN)) {
    debug_light = !debug_light;
  }
  if(debug_light) {
    if(Input::Get().isKeyJustDown(KEYBOARD_UP)) {
      propagateLight(true);
    }
  } else {
      propagateLight(false);
  }


  if(mEnableRaycast) {
    mPlayerRaycast = raycast(mCamera.transform().position(), 
                              mCamera.transform().right().normalized(), 8);
  } else {
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_DISABLE);
    if(mPlayerRaycast.impacted()) {
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.end,
                      1, 0, Rgba::gray, Rgba(50, 50, 50));
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba(128, 0, 0));
      Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.contact.position,
                      1, 0, Rgba::red, Rgba::white);
      Debug::drawPoint(mPlayerRaycast.contact.position, 0.2f, 0, Rgba::red);
    } else {
      Debug::drawLine(mPlayerRaycast.start, mPlayerRaycast.end,
                      1, 0, Rgba::green, Rgba::green);
    }

    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);
  }

  // Debug::drawPoint(mCamera.transfrom().position() + mCamera.transfrom().right(), .03f, 0);
  Debug::drawBasis(mCamera.transform().position() + mCamera.transform().right(),
                   vec3::right * .05f, vec3::up * .05f, vec3::forward * .05f, 0);
  if(mPlayerRaycast.impacted()) {
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_ENABLE);

    aabb3 innerbounds = mPlayerRaycast.contact.block.bounds();
    innerbounds = { innerbounds.mins - vec3{.03f}, innerbounds.maxs + vec3{.03f}};
    Debug::drawCube(innerbounds, mat44::identity, true, 0, Rgba::white);

    {
     
      /*
       *     2 ----- 1
       *    /|      /|
       *   3 ----- 0 |
       *   | 6 ----|-5
       *   |/      |/             x  
       *   7 ----- 4         y___/
       */  
      vec3 v[8] = {
        vec3{ innerbounds.mins.x, innerbounds.mins.y, innerbounds.maxs.z },
        vec3{ innerbounds.maxs.x, innerbounds.mins.y, innerbounds.maxs.z },
        vec3{ innerbounds.maxs.x, innerbounds.maxs.y, innerbounds.maxs.z },
        vec3{ innerbounds.mins.x, innerbounds.maxs.y, innerbounds.maxs.z },

        vec3{ innerbounds.mins.x, innerbounds.mins.y, innerbounds.mins.z },
        vec3{ innerbounds.maxs.x, innerbounds.mins.y, innerbounds.mins.z },
        vec3{ innerbounds.maxs.x, innerbounds.maxs.y, innerbounds.mins.z },
        vec3{ innerbounds.mins.x, innerbounds.maxs.y, innerbounds.mins.z },
      };

      Rgba tintColor(50, 20, 20);
      if(mPlayerRaycast.contact.normal.x == 1) {
        Debug::drawQuad(v[5], v[6], v[2], v[1], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.x == -1) {
        Debug::drawQuad(v[7], v[4], v[0], v[3], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.y == 1) {
        Debug::drawQuad(v[6], v[7], v[3], v[2], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.y == -1) {
        Debug::drawQuad(v[5], v[4], v[0], v[1], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.z == 1) {
        Debug::drawQuad(v[3], v[0], v[1], v[2], 0, tintColor);
      }
      if(mPlayerRaycast.contact.normal.z == -1) {
        Debug::drawQuad(v[6], v[5], v[4], v[7], 0, tintColor);
      }
    }
    Debug::setDepth(Debug::eDebugDrawDepthMode::DEBUG_DEPTH_DISABLE);

  }

}

void World::onRender(VoxelRenderer& renderer) const {
  renderer.setCamera(mCamera);
  for(auto& [coords, chunk]: mActiveChunks) {
    if(chunk->invalid()) continue;
    renderer.issueChunk(chunk);
  }

  renderer.onRenderFrame(*RHIDevice::get()->defaultRenderContext());
  renderer.onRenderGui(*RHIDevice::get()->defaultRenderContext());
}

void World::onDestroy() {
  std::vector<Chunk*> chunks;
  chunks.reserve(mActiveChunks.size());

  for(const auto& [_, chunk]: mActiveChunks) {
    if(chunk->valid()) {
      chunks.push_back(chunk);
    }
  }

  for(Chunk* c: chunks) {
    c->onDestroy();
    freeChunk(c);
  }

}

bool World::activateChunk(const ChunkCoords coords) {
  Chunk* chunk = allocChunk(coords);
  chunk->onInit();
  registerChunkToWorld(chunk);
  chunk->afterRegisterToWorld();
  return true;
}

bool World::deactivateChunk(const ChunkCoords coords) {
  Chunk* chunk = unregisterChunkFromWorld(coords);
  if(chunk->invalid()) {
    return false;
  }
  chunk->onDestroy();
  freeChunk(chunk);
  return true;
}

void World::registerChunkToWorld(Chunk* chunk) {
#ifdef _DEBUG
  {
    auto kv = mActiveChunks.find(chunk->coords());
    EXPECTS(kv == mActiveChunks.end() || 
            kv->second->invalid());
  }
#endif

  chunk->onRegisterToWorld(this);
  mActiveChunks[chunk->coords()] = chunk;
}

owner<Chunk*> World::unregisterChunkFromWorld(const ChunkCoords& coords) {
  auto iter = mActiveChunks.find(coords);
  if(iter == mActiveChunks.end()) {
    return Chunk::invalidIter().chunk();
  };
  Chunk* chunk = iter->second;
  iter->second = Chunk::invalidIter().chunk();

  ENSURES(chunk != nullptr);
  if(chunk->valid()) {
    chunk->onUnregisterFromWorld();
  }
  return chunk;
}

Chunk* World::findChunk(const ChunkCoords& coords) {
  const auto iter = mActiveChunks.find(coords);
  return iter == mActiveChunks.end() ? Chunk::invalidIter().chunk() : iter->second;
}

const Chunk* World::findChunk(const ChunkCoords& coords) const {
  return const_cast<World*>(this)->findChunk(coords);
}

Chunk* World::findChunk(const vec3& worldPosition) {

  ChunkCoords coords = ChunkCoords::fromWorld(worldPosition);
  return findChunk(coords);

}

World::raycast_result_t World::raycast(const vec3& start, const vec3& dir, float maxDist) {
  constexpr float kRaycastStepSize = 0.01f;
  raycast_result_t result;

  result.start = start;
  result.end = start + dir * maxDist;
  result.dir = dir;
  result.maxDist = maxDist;

  Chunk* chunk = findChunk(start);

  // fail to find the chunk

  if(chunk->invalid()) return result;

  // found chunk, start ray casting

  Chunk::BlockIter prev = chunk->blockIter(start);
  // hit start block
  if(Input::Get().isKeyJustDown('Z')) {
    Debug::drawCube(prev.bounds(), mat44::identity, true);
  }
  if(prev->opaque()) {
    result.contact = { start, -dir, 0, 0, prev };
    return result;
  }

  aabb3 prevBounds = prev.bounds();
  for(float dist = kRaycastStepSize; dist <= maxDist; dist += kRaycastStepSize) {

    vec3 currentPos = start + dist * dir;

    if(prevBounds.contains(currentPos)) continue;

    if(!chunk->bounds().contains(currentPos)) {
      chunk = findChunk(currentPos);
      // GUARANTEE_RECOVERABLE(chunk->valid(), "got invalid chunk");
    }

    Chunk::BlockIter next = chunk->blockIter(currentPos);
    aabb3 bounds = next.bounds();

    if(next->opaque()) {
      ray3 ray {start, dir};
      contact3 c = ray.intersect(bounds);

      result.contact = { 
        c.position, c.normal, 
        dist / maxDist, dist, next };
      return result;
    }

    prev = next;
    prevBounds = bounds;

  }

  // else missed
  return result;
}

void World::submitDirtyBlock(const Chunk::BlockIter& block) {
  mLightDirtyList.push_back(block);
}

owner<Mesh*> World::aquireDebugLightDirtyMesh() const {
  Mesher ms;

  ms.begin(DRAW_TRIANGES);
  for(auto block: mLightDirtyList) {
    ms.color(Rgba::yellow);
    ms.cube(block.bounds().center(), vec3::one * .01f);
  }
  ms.end();

  return ms.createMesh<>();
}

void World::updateChunks() {

  for(auto& [coords, chunk]: mActiveChunks) {
    if(chunk->valid()) chunk->onUpdate();
  }
  
}

void World::manageChunks() {

  uint activatedChunkCount = 0;
  ChunkCoords playerChunkCoords = ChunkCoords::fromWorld(playerPosition());
  
  for(ChunkCoords& idx: sChunkActivationVisitingPattern) {
  
    ChunkCoords coords = playerChunkCoords + idx;
  
    Chunk* chunk = findChunk(coords);
    if(chunk->invalid()) {
      activateChunk(coords);
      activatedChunkCount++;
    }
  
    if(activatedChunkCount == Config::kMaxChunkActivatePerFrame) break;
  }

  uint reconstructedMeshCount = 0;
  for(ChunkCoords& idx: sChunkActivationVisitingPattern) {

    ChunkCoords coords = playerChunkCoords + idx;

    Chunk* chunk = findChunk(coords);
    if(chunk->valid() && chunk->isDirty()) {
      if(chunk->reconstructMesh()) {
        reconstructedMeshCount++;
      }
    }

    if(reconstructedMeshCount == Config::kMaxChunkReconstructMeshPerFrame) break;
  }

  uint deactivatedChunkCount = 0;
  for(ChunkCoords& idx: sChunkDeactivationVisitingPattern) {
  
    ChunkCoords coords = playerChunkCoords + idx;
    if(deactivateChunk(coords)) {
      deactivatedChunkCount++;
    }
  
    if(deactivatedChunkCount == Config::kMaxChunkDeactivatePerFrame) break;
  }

}

static void updateBlockLight(const Chunk::BlockIter& iter, std::deque<Chunk::BlockIter>& pending) {
  EXPECTS(iter.valid());
  uint8_t oldOutdoorLight = iter->outdoorLight();
  uint8_t newOutdoorLight = 0;

  uint8_t oldIndoorLight = iter->indoorLight();
  uint8_t newIndoorLight = iter->type().emissive();

  Chunk::BlockIter neighbors[6] = {
    iter.nextNegX(),
    iter.nextNegY(),
    iter.nextNegZ(),
    iter.nextPosX(),
    iter.nextPosY(),
    iter.nextPosZ()
  };

  bool opaque = iter->opaque();
  
  // init update outdoor lighting
  Chunk::BlockIter topBlock = neighbors[5];
  if((topBlock->exposedToSky() && !opaque)
     || !topBlock.valid() // I am detecting very top block
     ) {
    newOutdoorLight = Block::kMaxOutdoorLight;
  }

  if(!opaque) {
    for(auto block: neighbors){
      if(block.valid()) {
        if(newIndoorLight + 1 < block->indoorLight()) {
          Block& b = *block;
          newIndoorLight = b.indoorLight() - 1;
        }
        if(newOutdoorLight + 1 < block->outdoorLight()) {
          Block& b = *block;
          uint xx = block->outdoorLight() - 1;
          newOutdoorLight = xx;
        }
      }
    }
  }


  bool needToDirtyNeighbors = (oldIndoorLight != newIndoorLight) || (oldOutdoorLight != newOutdoorLight);

  if(needToDirtyNeighbors) {
    iter->setIndoorLight(newIndoorLight);
    iter->setOutdoorLight(newOutdoorLight);
    iter.chunk->setDirty();
    for(auto block: neighbors) {
      if(!block->dirty() && !block->opaque() && block.valid()) {
        block->setDirty();
        pending.push_back(block);
      }
    }
  }

  iter->clearDirty();
  
}

void World::propagateLight(bool step) {

  if(step) {
    Log::logf("dirty block count: %u", mLightDirtyList.size());
    std::deque<Chunk::BlockIter> blocksToUpdate;
    std::swap(blocksToUpdate, mLightDirtyList);

    while(!blocksToUpdate.empty()) {
      auto block = blocksToUpdate.front();
      blocksToUpdate.pop_front();
      updateBlockLight(block, mLightDirtyList);
    }
  } else {
    while(!mLightDirtyList.empty()) {
      // OutputDebugStringA(Stringf("dirty block count: %u\n", mLightDirtyList.size()).c_str());
      Chunk::BlockIter block = mLightDirtyList.front();
      mLightDirtyList.pop_front();
      EXPECTS(block.valid());
      updateBlockLight(block, mLightDirtyList);
    }
  }

}

vec3 World::playerPosition() {
  return mCamera.transform().position();
}

std::vector<ChunkCoords> World::sChunkActivationVisitingPattern{};
std::vector<ChunkCoords> World::sChunkDeactivationVisitingPattern{};

void World::reconstructChunkVisitingPattern() {
  int halfRange = (int)floor(Config::kMinDeactivateDistance / float(std::max(Chunk::kSizeY, Chunk::kSizeX)));

  int activateChunkDist = (int)floor(Config::kMaxActivateDistance / float(std::max(Chunk::kSizeY, Chunk::kSizeX)));

  int activateChunkDist2 = activateChunkDist * activateChunkDist;
  EXPECTS(halfRange > 0);

  sChunkActivationVisitingPattern.reserve(halfRange * halfRange);
  sChunkDeactivationVisitingPattern.reserve(halfRange * halfRange);

  for(int j = -halfRange; j < halfRange; ++j) {
    for(int i = -halfRange; i < halfRange; ++i) {
      ivec2 coord{i, j};
      if(coord.magnitude2() <= activateChunkDist2) {
        sChunkActivationVisitingPattern.emplace_back(coord);
      } else {
        sChunkDeactivationVisitingPattern.emplace_back(coord);
      }
    }
  }

  std::sort(sChunkActivationVisitingPattern.begin(), 
            sChunkActivationVisitingPattern.end(), 
            [](const ivec2& lhs, const ivec2& rhs) {
    return lhs.magnitude2() < rhs.magnitude2();
  });

  std::sort(sChunkDeactivationVisitingPattern.begin(), 
            sChunkDeactivationVisitingPattern.end(), 
            [](const ivec2& lhs, const ivec2& rhs) {
    return lhs.magnitude2() > rhs.magnitude2();
  });

  ENSURES(sChunkActivationVisitingPattern[0] == ivec2::zero);
}
