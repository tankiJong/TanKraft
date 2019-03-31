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
#include "Engine/Math/Noise/SmoothNoise.hpp"

void World::onInit() {

  if(sChunkActivationVisitingPattern.empty()) reconstructChunkVisitingPattern();


  Debug::setDepth(Debug::DEBUG_DEPTH_DISABLE);
  Debug::drawBasis(Transform());

  
}

void World::onInput() {
  Debug::drawText("hello text", 4, vec3::zero, 0);
  Debug::drawText2("hello text2d", 16, vec2::zero, 0);

  if(Input::Get().isKeyDown('T')) {
    gGameClock->scale(10000);
  } else {
    gGameClock->scale(200);
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

void World::onUpdate(const vec3& viewPosition) {
  float dt = (float)GetMainClock().frame.second;
  mCurrentViewPosition = viewPosition;
  {
    ImGui::Begin("Environment Noises");
    {
      float noise = Compute1dPerlinNoise(
        float(gGameClock->total.second) / 86400.f, .003, 9);
      noise = rangeMap(noise, .5f, .9f, 0.f, 1.f);
      noise = clampf01(noise);
      size_t writeIndex = mWeatherNoiseSample.push(noise);
      ImGui::PlotLines("Weather", (const float*)&mWeatherNoiseSample, 
                       (int)mWeatherNoiseSample.capacity(), (int)writeIndex, 
                       "", 0.f, 1.f, ImVec2{0, 80});
    }

    {
      float noise = Compute1dPerlinNoise(float(gGameClock->total.second) / 86400.f, .005, 9);
      noise = rangeMap(noise, -1.f, 1.f, .8f, 1.f);
      // noise = clampf01(noise);
      // noise = 0;
      size_t writeIndex = mFlameNoiseSample.push(noise);
      ImGui::PlotLines("Flame Flicker", (const float*)&mFlameNoiseSample, 
                       (int)mFlameNoiseSample.capacity(), (int)writeIndex, 
                       "", .8f, 1.f, ImVec2{0, 80});
      
    }
    ImGui::End();
  }



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

}

void World::onRender(VoxelRenderer& renderer) const {
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

// Chunk* World::findChunk(const ChunkCoords& coords) {
// }

Chunk* World::findChunk(const ChunkCoords& coords) const {
   const auto iter = mActiveChunks.find(coords);
   return iter == mActiveChunks.end() ? Chunk::invalidIter().chunk() : iter->second;
}

Chunk* World::findChunk(const vec3& worldPosition) const {

  ChunkCoords coords = ChunkCoords::fromWorld(worldPosition);
  return findChunk(coords);

}

raycast_result_t World::raycast(const vec3& start, const vec3& dir, float maxDist) const {
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

void World::onEndFrame() {
  if(!mWeatherNoiseSample.empty()) {
    mWeatherNoiseSample.pop();
  }
  
  if(!mFlameNoiseSample.empty()) {
    mFlameNoiseSample.pop();
  }
}

bool World::collide(CollisionSphere& target) const {
  EXPECTS(target.radius <= .5f);

  vec3 targetCenter = target.center();
  vec3 originalCenter = targetCenter;
  Chunk* chunk = findChunk(targetCenter);
  EXPECTS(chunk != nullptr);

  Chunk::BlockIter center = chunk->blockIter(targetCenter);
  vec3 centerPosition = center.centerPosition();
  EXPECTS(center.valid());

  Chunk::BlockIter 
    iterpx = center, iternx = center, 
    iterpy = center, iterny = center, 
    iterpz = center, iternz = center;

  bool collided = false;

  // x
  {
    bool collideFirst = false;
    {
      auto iter = center.nextPosX();
      iterpx = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{1, 0, 0};
        float ds = position.x - targetCenter.x;
        if(ds < .5f + target.radius) {
          targetCenter.x = position.x - .5f - target.radius;
          collided = true;
          collideFirst = true;
        }
      }
    } if(!collideFirst) {
      auto iter = center.nextNegX();
      iternx = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{-1, 0, 0};
        float ds = targetCenter.x - position.x;
        if(ds < .5f + target.radius) {
          targetCenter.x = position.x + .5f + target.radius;
          collided = true;
        }
      }
    }
  }

  // y
  {
    bool collideFirst = false;
    {
      auto iter = center.nextPosY();
      iterpy = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{0, 1, 0};
        float ds = position.y - targetCenter.y;
        if(ds < .5f + target.radius) {
          targetCenter.y = position.y - .5f - target.radius;
          collided = true;
          collideFirst = true;
        }
      }
    } if(!collideFirst) {
      auto iter = center.nextNegY();
      iterny = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{0, -1, 0};
        float ds = targetCenter.y - position.y;
        if(ds < .5f + target.radius) {
          targetCenter.y = position.y + .5f + target.radius;
          collided = true;
        }
      }
    }
  }

  // z
  {
    bool collideFirst = false;
    {
      auto iter = center.nextPosZ();
      iterpz = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{0, 0, 1};
        float ds = position.z - targetCenter.z;
        if(ds < .5f + target.radius) {
          targetCenter.z = position.z - .5f - target.radius;
          collided = true;
          collideFirst = true;
        }
      }
    } if(!collideFirst) {
      auto iter = center.nextNegZ();
      iternz = iter;
      if(iter->opaque()) {
        vec3 position = centerPosition + vec3{0, 0, -1};
        float ds = targetCenter.z - position.z;
        if(ds < .5f + target.radius) {
          targetCenter.z = position.z + .5f + target.radius;
          collided = true;
        }
      }
    }
  }
  // ---------- early out 1 ----------
  if(collided) {
    target.target->localTranslate(targetCenter - originalCenter);
    return collided;
  }

  Chunk::BlockIter
    // const x
    iterpypz = center, iterpynz = center,
    iternypz = center, iternynz = center,
    // const y
    iterpxpz = center, iterpxnz = center,
    iternxpz = center, iternxnz = center,
    // const z
    iternxpy = center, iternxny = center,
    iterpxpy = center, iterpxny = center;


  /*
   *   1 - 2
   *   |   |
   *   4 - 3
   */
  
  // yz - const x
  {
    iterpypz = iterpy.nextPosZ();
    iternypz = iterny.nextPosZ();
    iternynz = iterny.nextNegZ();
    iterpynz = iterpy.nextNegZ();

    vec2 centeryz = centerPosition.yz();

    // 1, 3
    if(iterpypz->opaque()){
      vec2 delta = vec2{.5f, .5f};
      vec2 corner1 = centeryz + delta;
      float distance1 = targetCenter.yz().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centeryz - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{0, direction.x, direction.y};
        collided = true;
      } else if(iternynz->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centeryz - delta;
        float distance2 = targetCenter.yz().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centeryz - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{0, direction.x, direction.y};
          collided = true;
        }
      }
    }

    // 2, 4
    if(iternypz->opaque()){
      vec2 delta = vec2{.5f, -.5f};
      vec2 corner1 = centeryz + delta;
      float distance1 = targetCenter.yz().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centeryz - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{0, direction.x, direction.y};
        collided = true;
      } else if(iterpynz->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centeryz - delta;
        float distance2 = targetCenter.yz().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centeryz - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{0, direction.x, direction.y};
          collided = true;
        }
      }
    }
  }
  // xz - const y
  {
    iternxpz = iternx.nextPosZ();
    iterpxpz = iterpx.nextPosZ();
    iterpxnz = iterpx.nextNegZ();
    iternxnz = iternx.nextNegZ();
    
    vec2 centerxz = centerPosition.xz();

    // 1, 3
    if(iternxpz->opaque()){
      vec2 delta = vec2{.5f, .5f};
      vec2 corner1 = centerxz + delta;
      float distance1 = targetCenter.xz().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centerxz - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{direction.x, 0, direction.y};
        collided = true;
      } else if(iterpxnz->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centerxz - delta;
        float distance2 = targetCenter.xz().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxz - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, 0, direction.y};
          collided = true;
        }
      }
    }

    // 2, 4
    if(iterpxpz->opaque()){
      vec2 delta = vec2{.5f, -.5f};
      vec2 corner1 = centerxz + delta;
      float distance1 = targetCenter.xz().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centerxz - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{direction.x, 0, direction.y};;
        collided = true;
      } else if(iternxnz->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centerxz - delta;
        float distance2 = targetCenter.xz().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxz - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, 0, direction.y};;
          collided = true;
        }
      }
    }
  }
  // xy - const z
  {
    iterpxpy = iterpx.nextPosY();
    iterpxny = iterpx.nextNegY();
    iternxny = iternx.nextNegY();
    iternxpy = iternx.nextPosY();
    
    vec2 centerxy = centerPosition.xy();

    // 1, 3
    if(iterpxpy->opaque()){
      vec2 delta = vec2{.5f, .5f};
      vec2 corner1 = centerxy + delta;
      float distance1 = targetCenter.xy().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centerxy - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{direction.x, direction.y, 0};
        collided = true;
      } else if(iternxny->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centerxy - delta;
        float distance2 = targetCenter.xy().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxy - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, direction.y, 0};
          collided = true;
        }
      }
    } else {
      vec2 delta = vec2{.5f, .5f};
      if(iternxny->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centerxy - delta;
        float distance2 = targetCenter.xy().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxy - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, direction.y, 0};
          collided = true;
        }
      }
    }

    // 2, 4
    if(iterpxny->opaque()){
      vec2 delta = vec2{.5f, -.5f};
      vec2 corner1 = centerxy + delta;
      float distance1 = targetCenter.xy().distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec2 direction = (centerxy - corner1).normalized();
        direction = direction * ds;
        targetCenter += vec3{direction.x, direction.y, 0};
        collided = true;
      } else if(iternxpy->opaque()) {
        // not collided, check the other side
        vec2 corner2 = centerxy - delta;
        float distance2 = targetCenter.xy().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxy - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, direction.y, 0};
          collided = true;
        }
      }
    } else {
      if(iternxpy->opaque()) {
        // not collided, check the other side
        vec2 delta = vec2{.5f, -.5f};
        vec2 corner2 = centerxy - delta;
        float distance2 = targetCenter.xy().distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec2 direction = (centerxy - corner2).normalized();
          direction = direction * ds;
          targetCenter += vec3{direction.x, direction.y, 0};
          collided = true;
        }
      }
    }
  }

  // ---------- early out 2 ----------  
  if(collided) {
    target.target->localTranslate(targetCenter - originalCenter);
    return collided;
  }

  /*
   *     0 ----- 1
   *    /|      /|
   *   3 ----- 2 |
   *   | 4 ----|-5
   *   |/      |/             x  
   *   7 ----- 6         y___/
   */  
  Chunk::BlockIter
    iterpxpypz = iterpxpy.nextPosZ(), // 0
    iternxnynz = iternxny.nextNegZ(), // 6

    iterpxnypz = iterpxny.nextPosZ(), // 1
    iternxpynz = iternxpy.nextNegZ(), // 7

    iternxnypz = iternxny.nextPosZ(), // 2
    iterpxpynz = iterpxpy.nextNegZ(), // 4
  
    iternxpypz = iternxpy.nextPosZ(), // 3
    iterpxnynz = iterpxny.nextNegZ(); // 5

  {
    // 0, 6
    if(iterpxpypz->opaque()) {
      vec3 delta = vec3{.5f, .5f, .5f};
      vec3 corner1 = centerPosition + delta;
      float distance1 = targetCenter.distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec3 direction = (centerPosition - corner1).normalized();
        direction = direction * ds;
        targetCenter += direction;
        collided = true;
      } else if(iternxnynz->opaque()) {
        // not collided, check the other side
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    } else {
      if(iternxnynz->opaque()) {
        // not collided, check the other side
        vec3 delta = vec3{.5f, .5f, .5f};
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    }
  }

  {
    // 1, 7
    if(iternxpypz->opaque()){
      vec3 delta = vec3{.5f, -.5f, .5f};
      vec3 corner1 = centerPosition + delta;
      float distance1 = targetCenter.distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec3 direction = (centerPosition - corner1).normalized();
        direction = direction * ds;
        targetCenter += direction;
        collided = true;
      } else if(iternxpynz->opaque()) {
        // not collided, check the other side
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    } else {
      if(iternxpynz->opaque()) {
        // not collided, check the other side
        vec3 delta = vec3{.5f, -.5f, .5f};
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    }
  }

  {
    // 2, 4
    if(iternxnypz->opaque()){
      vec3 delta = vec3{-.5f, -.5f, .5f};
      vec3 corner1 = centerPosition + delta;
      float distance1 = targetCenter.distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec3 direction = (centerPosition - corner1).normalized();
        direction = direction * ds;
        targetCenter += direction;
        collided = true;
      } else if(iterpxpynz->opaque()) {
        // not collided, check the other side
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    } else {
      if(iterpxpynz->opaque()) {
        // not collided, check the other side
      vec3 delta = vec3{-.5f, -.5f, .5f};
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    }
  }

  {
    // 3, 5
    if(iternxnypz->opaque()){
      vec3 delta = vec3{-.5f, .5f, .5f};
      vec3 corner1 = centerPosition + delta;
      float distance1 = targetCenter.distance(corner1);
      if(distance1 < target.radius) {
        float ds = target.radius - distance1;
        vec3 direction = (centerPosition - corner1).normalized();
        direction = direction * ds;
        targetCenter += direction;
        collided = true;
      } else if(iterpxnynz->opaque()) {
        // not collided, check the other side
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    } else {
      if(iterpxnynz->opaque()) {
        // not collided, check the other side
        vec3 delta = vec3{-.5f, .5f, .5f};
        vec3 corner2 = centerPosition - delta;
        float distance2 = targetCenter.distance(corner2);
        if(distance2 < target.radius) {
          float ds = target.radius - distance2;
          vec3 direction = (centerPosition - corner2).normalized();
          direction = direction * ds;
          targetCenter += direction;
          collided = true;
        }
      }
    }
  }

  if(collided) {
    target.target->localTranslate(targetCenter - originalCenter);
  }
  return collided;
}

void World::updateChunks() {

  for(auto& [coords, chunk]: mActiveChunks) {
    if(chunk->valid()) chunk->onUpdate();
  }
  
}

void World::manageChunks() {

  uint activatedChunkCount = 0;
  ChunkCoords playerChunkCoords = ChunkCoords::fromWorld(viewPosition());
  
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
          uint8_t xx = b.outdoorLight() - 1;
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
      if(!block->lightDirty() && !block->opaque() && block.valid()) {
        block->setLightDirty();
        pending.push_back(block);
      }
    }
  }

  iter->clearLightDirty();
  
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

vec3 World::viewPosition() {
  return mCurrentViewPosition;
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