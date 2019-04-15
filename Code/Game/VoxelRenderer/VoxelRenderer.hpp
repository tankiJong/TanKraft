#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Graphics/RHI/Texture.hpp"
#include "Engine/Graphics/Model/Mesh.hpp"
#include "Engine/Renderer/RenderGraph/RenderGraph.hpp"
#include "Game/World/Chunk.hpp"
#include "Game/World/GPUVolume.hpp"

class RenderScene;
class Camera;


struct ChunkRenderData {
  Mesh* mesh = nullptr;
  mat44 model;
  const Chunk* chunk;
};

class VoxelRenderer: public Renderer {
public:
  
  void onLoad(RHIContext& ctx) override;

  void onRenderFrame(RHIContext& ctx) override;
  void onRenderGui(RHIContext& ctx) override;

  void issueChunk(const Chunk* chunk);
  void updatePlayerPosition(vec3 playerPosition);
  void setCamera(const Camera& cam) {
    mCamera = &cam;
  };

  void setWorld(const World* world) {
    mWorld = world;
    mWorldVolume.setWorld(world);
  }

  void raytracing(const Chunk* chunk);
  ~VoxelRenderer();
protected:

  struct frame_data_t {
    float gTime;
	  float gFrameCount;
    float gRoughness;
    float gMetalness;
    float gPlanetRadius{100000};
    vec3  gSunDir;
    float gPlanetAtmosphereThickness{25000};
    vec3  gRayleigh{5.5e1f, 13.0e1f, 22.4e1f};
    float gSunPower = 20.f;
    vec3  gMie{21e1, 21e1, 21e1};
    vec2  gScatterThickness{698, 2500};
    vec2  gViewDistance;
    vec4  gWorldConstant; // x: lighting strike, y: flame flicker
  };


  void updateFrameConstant(RHIContext& ctx);
  void updateViewConstant(RHIContext& ctx);
  void cleanBuffers(RHIContext& ctx);
  void generateGbuffer(RHIContext& ctx);
  void deferredShading(RHIContext& ctx);
  void copyToBackBuffer(RHIContext& ctx);

  void constructFrameMesh();
  void constructTestSphere();
  void defineRenderPasses();

  void cleanupFrameData();
  // const buffers
  RHIBuffer::sptr_t   mCFrameData;
  RHIBuffer::sptr_t   mCCamera;
  RHIBuffer::sptr_t   mCModel;
  
  // G-buffers
  Texture2::sptr_t    mGAlbedo;
  Texture2::sptr_t    mGDepth;
  Texture2::sptr_t    mGPosition;
  Texture2::sptr_t    mGNormal;
  Texture2::sptr_t    mGTangent;
  Texture2::sptr_t    mGBiTangent;
  
  Texture2::sptr_t    mTexAO;
  // resource
  TypedBuffer::sptr_t mTLights;
  
  Texture2::sptr_t    mTFinal;

  frame_data_t mFrameData {0, 0 };
  Mesh* mFrameMesh = nullptr;
  const Camera* mCamera = nullptr;
  const World* mWorld = nullptr;
  RenderGraph mGraph;

  GPUVolume mWorldVolume;
  std::vector<ChunkRenderData> mFrameRenderData;
};
