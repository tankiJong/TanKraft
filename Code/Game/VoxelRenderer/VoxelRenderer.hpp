#pragma once
#include "Engine/Core/common.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Graphics/RHI/Texture.hpp"
#include "Engine/Graphics/Model/Mesh.hpp"
#include "Engine/Renderer/RenderGraph/RenderGraph.hpp"

class RenderScene;
class Camera;

class VoxelRenderer: public Renderer {
public:
  
  void onLoad(RHIContext& ctx) override;

  void onRenderFrame(RHIContext& ctx) override;

  void onRenderGui(RHIContext& ctx) override {};

  void setCamera(const Camera& cam) {
    mCamera = &cam;
  };

  ~VoxelRenderer();
protected:

  struct frame_data_t {
    float gTime;
	  float gFrameCount;
  };

  void cleanBuffers(RHIContext& ctx);
  void generateGbuffer(RHIContext& ctx);
  void deferredShading(RHIContext& ctx);
  void copyToBackBuffer(RHIContext& ctx);
  void constructFrameMesh();
  void updateFrameConstant(RHIContext& ctx);
  void updateViewConstant(RHIContext& ctx);
  void defineRenderPasses();
  // const buffers
  RHIBuffer::sptr_t mCFrameData;
  RHIBuffer::sptr_t mCCamera;
  RHIBuffer::sptr_t mCModel;

  // G-buffers
  Texture2::sptr_t mGAlbedo;
  Texture2::sptr_t mGDepth;
  Texture2::sptr_t mGPosition;
  Texture2::sptr_t mGNormal;

  // resource
  TypedBuffer::sptr_t mTLights;

  Texture2::sptr_t mTFinal;

  frame_data_t mFrameData {0, 0 };
  Mesh* mFrameMesh = nullptr;
  const Camera* mCamera = nullptr;

  RenderGraph mGraph;
};
