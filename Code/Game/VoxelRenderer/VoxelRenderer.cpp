#include "VoxelRenderer.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Engine/Application/Window.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"
#include "Engine/Graphics/RHI/FrameBuffer.hpp"
#include "Engine/Graphics/Program/Program.hpp"
#include "Engine/Graphics/Program/ProgramInst.hpp"
#include "Engine/Graphics/RHI/PipelineState.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Graphics/Camera.hpp"

#include "VoxelRenderer/DeferredShading_ps.h"
#include "VoxelRenderer/DeferredShading_vs.h"
#include "VoxelRenderer/GenGBuffer_ps.h"
#include "VoxelRenderer/GenGBuffer_vs.h"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Engine/Gui/ImGui.hpp"
#include "Engine/Math/Noise/SmoothNoise.hpp"
#include "Engine/Renderer/RenderGraph/RenderGraphBuilder.hpp"

// DFS TODO: add ConstBuffer class

void VoxelRenderer::onLoad(RHIContext& ctx) {
  auto size = Window::Get()->bounds().size();
  uint width = (uint)size.x;
  uint height = (uint)size.y;


  mTFinal = Texture2::create( width, height, TEXTURE_FORMAT_RGBA8, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mTFinal);

  mGAlbedo = Texture2::create( width, height, TEXTURE_FORMAT_RGBA8, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGAlbedo);

  mGPosition = Texture2::create( width, height, TEXTURE_FORMAT_RGBA16, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGPosition);

  mGNormal = Texture2::create( width, height, TEXTURE_FORMAT_RGBA16, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGNormal);

  mCFrameData = RHIBuffer::create(sizeof(frame_data_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCFrameData);

  mCCamera = RHIBuffer::create(sizeof(camera_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCCamera);

  mCModel = RHIBuffer::create(sizeof(mat44), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCModel);

  constructFrameMesh();

}

void VoxelRenderer::onRenderFrame(RHIContext& ctx) {

  updateFrameConstant(ctx);
  updateViewConstant(ctx);

  ctx.bindDescriptorHeap();

  //---------
  ctx.transitionBarrier(mGAlbedo.get(), RHIResource::State::RenderTarget);
  ctx.transitionBarrier(mGPosition.get(), RHIResource::State::RenderTarget);
  ctx.transitionBarrier(mGNormal.get(), RHIResource::State::RenderTarget);
  cleanBuffers(ctx);
  ctx.transitionBarrier(mGDepth.get(), RHIResource::State::DepthStencil);

  //---------
  generateGbuffer(ctx);

  //---------
  ctx.transitionBarrier(mTFinal.get(), 
                        RHIResource::State::RenderTarget);
  ctx.transitionBarrier(mGAlbedo.get(), RHIResource::State::ShaderResource);
  ctx.transitionBarrier(mGPosition.get(), RHIResource::State::ShaderResource);
  ctx.transitionBarrier(mGNormal.get(), RHIResource::State::ShaderResource);
  ctx.transitionBarrier(mGDepth.get(), RHIResource::State::ShaderResource);
  deferredShading(ctx);


  //---------
  ctx.transitionBarrier(mTFinal.get(), RHIResource::State::CopySource);
  ctx.transitionBarrier(RHIDevice::get()->backBuffer().get(), RHIResource::State::CopyDest);
  copyToBackBuffer(ctx);

  // RHIDevice::get()->present();
}

VoxelRenderer::~VoxelRenderer() {
  SAFE_DELETE(mFrameMesh);
}

void VoxelRenderer::cleanBuffers(RHIContext& ctx) {
  ctx.clearRenderTarget(mGAlbedo->rtv(), Rgba::black);
  ctx.clearRenderTarget(mGPosition->rtv(), Rgba::black);
  ctx.clearRenderTarget(mGNormal->rtv(), Rgba::gray);
  
  ctx.clearDepthStencilTarget(*mGDepth->dsv());
}

void VoxelRenderer::generateGbuffer(RHIContext& ctx) {
  SCOPED_GPU_EVENT(ctx, "Gen G-Buffer");

  FrameBuffer fbo;

  fbo.defineColorTarget(mGAlbedo, 0);
  fbo.defineColorTarget(mGNormal, 1);
  fbo.defineColorTarget(mGPosition, 2);
  fbo.defineDepthStencilTarget(mGDepth);
  

  S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/GenGBuffer");
  EXPECTS(prog != nullptr);

  S<ProgramInst> inst = GraphicsProgramInst::create(prog);
  {
    inst->setCbv(*mCFrameData->cbv(), 0);
    inst->setCbv(*mCCamera->cbv(), 1);
    inst->setCbv(*mCModel->cbv(), 2);

    static auto albedo = Resource<Texture2>::get("/Data/Images/Terrain_8x8.png");
    ctx.transitionBarrier(albedo.get(), RHIResource::State::ShaderResource);
    inst->setSrv(albedo->srv(), 0);
    // inst->setSrv()
  }

  GraphicsState::Desc desc;
  {
    desc.setProgram(prog);
    desc.setFboDesc(fbo.desc());
    desc.setPrimTye(GraphicsState::PrimitiveType::Triangle);
    desc.setRootSignature(prog->rootSignature());
    desc.setVertexLayout(VertexLayout::For<vertex_lit_t>());
  }
  
  GraphicsState::sptr_t gs = GraphicsState::create(desc);
  ctx.setGraphicsState(*gs);

  inst->apply(ctx, true);
  ctx.setFrameBuffer(fbo);

  mCModel->updateData(mat44::identity);
  mFrameMesh->bindForContext(ctx);
  for(const draw_instr_t& instr: mFrameMesh->instructions()) {
    ctx.setPrimitiveTopology(instr.prim);
    if(instr.useIndices) {
      ctx.drawIndexed(0, instr.startIndex, instr.elementCount);
    } else {
      ctx.draw(instr.startIndex, instr.elementCount);
    }
  }
}

void VoxelRenderer::deferredShading(RHIContext& ctx) {
  SCOPED_GPU_EVENT(ctx, "Deferred Shading");

  FrameBuffer fbo;

  fbo.defineColorTarget(mTFinal, 0);

  S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/DeferredShading");
  EXPECTS(prog != nullptr);

  S<ProgramInst> inst = GraphicsProgramInst::create(prog);
  {
    inst->setCbv(*mCFrameData->cbv(), 0);
    inst->setCbv(*mCCamera->cbv(), 1);
    inst->setCbv(*mCModel->cbv(), 2);

    inst->setSrv(mGAlbedo->srv(), 0);
    inst->setSrv(mGNormal->srv(), 1);
    inst->setSrv(mGPosition->srv(), 2);
    inst->setSrv(mTLights->srv(), 3);
    // inst->setSrv()
  }

  GraphicsState::Desc desc;
  {
    desc.setProgram(prog);
    desc.setFboDesc(fbo.desc());
    desc.setPrimTye(GraphicsState::PrimitiveType::Triangle);
    desc.setRootSignature(prog->rootSignature());
    desc.setVertexLayout(VertexLayout::For<vertex_lit_t>());
  }
  
  GraphicsState::sptr_t gs = GraphicsState::create(desc);
  ctx.setGraphicsState(*gs);

  inst->apply(ctx, true);
  ctx.setFrameBuffer(fbo);
  ctx.draw(0, 3);
}

void VoxelRenderer::copyToBackBuffer(RHIContext& ctx) {
  ctx.copyResource(*mTFinal, *RHIDevice::get()->backBuffer());
}

void VoxelRenderer::constructFrameMesh() {

  constexpr float kBlockSize = 1.f; 

  mTLights = TypedBuffer::For<light_info_t>(100, RHIResource::BindingFlag::ShaderResource);
  
  Mesher ms;
  ms.begin(DRAW_TRIANGES);

  auto addBlock = [&](const vec3& center, const vec3& dimension) {
    vec3 bottomCenter = center - vec3::up * dimension.y * .5f;
    float dx = dimension.x * .5f, dy = dimension.y * .5f, dz = dimension.z * .5f;

    std::array<vec3, 8> vertices = {
      bottomCenter + vec3{ -dx, 2.f * dy, -dz },
      bottomCenter + vec3{ dx, 2.f * dy, -dz },
      bottomCenter + vec3{ dx, 2.f * dy,  dz },
      bottomCenter + vec3{ -dx, 2.f * dy,  dz },

      bottomCenter + vec3{ -dx, 0, -dz },
      bottomCenter + vec3{ dx, 0, -dz },
      bottomCenter + vec3{ dx, 0,  dz },
      bottomCenter + vec3{ -dx, 0,  dz }
    };

    ms.quad(vertices[0], vertices[1], vertices[2], vertices[3], 
        {0,.875}, {.125, .875}, {.125, 1}, {0, 1});
    ms.quad(vertices[4], vertices[7], vertices[6], vertices[5],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    ms.quad(vertices[4], vertices[5], vertices[1], vertices[0],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    ms.quad(vertices[5], vertices[6], vertices[2], vertices[1],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    ms.quad(vertices[6], vertices[7], vertices[3], vertices[2],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
    ms.quad(vertices[7], vertices[4], vertices[0], vertices[3],
        {.25, .25}, {.25, .375}, {.375, .375}, {.37, .25});
  };

  for(int j = 0; j < 10; ++j) {
    for(int i = 0; i < 10; ++i) {
      int kmax = abs(Compute2dPerlinNoise(i, j, 10, 3)) * 5.f + 1;
      for(int k = 0; k < kmax; k++) {
        vec2 centerPosition { i * kBlockSize, j * kBlockSize };
        addBlock(
          vec3{ centerPosition.x, k * kBlockSize, centerPosition.y }, 
          vec3{ kBlockSize });
      }
    }
  }

  ms.end();

  mFrameMesh = ms.createMesh();
}

void VoxelRenderer::updateFrameConstant(RHIContext& ctx) {
  mFrameData.gFrameCount++;
  mFrameData.gTime = GetMainClock().total.second;
  mCFrameData->updateData(mFrameData);

  mGDepth = RHIDevice::get()->depthBuffer();

  for(int j = 0; j < 10; ++j) {
    for(int i = 0; i < 10; ++i) {
      if(i%2 != 0 || j % 2 != 0) continue;
      vec2 centerPosition { i * 1.f, j * 1.f };

      light_info_t light;

      vec3 lightPosition = {centerPosition.x, sinf(GetMainClock().total.second) * 2 + 3, centerPosition.y};
      Rgba lightColor = Rgba(vec3{getRandomf01(), getRandomf01(), getRandomf01()});
      light.asPointLight(
        lightPosition,
        4, vec3{0,0,1}, 
        lightColor);
      mTLights->set(j*10 + i, light);

      // mat44 view = mCamera->view();
      // mat44 proj = mCamera->projection();
      // mat44 trans = mat44::makeTranslation(lightPosition);
      // ImGuizmo::DrawCube((float*)&view, (float*)&proj, (float*)&trans);
    }
  }
  mTLights->uploadGpu();
}

void VoxelRenderer::updateViewConstant(RHIContext& ctx) {
  mCCamera->updateData(mCamera->ubo());
  aabb2 bounds = { vec2::zero, { Window::Get()->bounds().width(), Window::Get()->bounds().height()} };
  ctx.setViewport(bounds);
  ctx.setScissorRect(bounds);
}

void VoxelRenderer::defineRenderPasses() {
  // mGraph.definePass("G-Buffer", [&](RenderGraphBuilder& builder) {
  //   builder.readCbv(mCFrameData, 0);
  //   builder.readCbv(mCCamera, 1);
  //   builder.readCbv(mCModel, 2);
  //
  //   auto albedo = Resource<Texture2>::get("/Data/Images/Terrain_8x8.png");
  //   builder.readSrv(albedo, 0);
  //
  //   builder.writeRtv()
  //   return []
  // });
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/GenGBuffer") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_VERTEX).setFromBinary(gGenGBuffer_vs, sizeof(gGenGBuffer_vs));
  prog->stage(SHADER_TYPE_FRAGMENT).setFromBinary(gGenGBuffer_ps, sizeof(gGenGBuffer_ps));
  prog->compile();

  RenderState state;
  // state.isWriteDepth = FLAG_FALSE;
  state.depthMode = COMPARE_LESS;
  // // state.depthMode = COMPARE_LEQUAL;
  // state.colorBlendOp = BLEND_OP_ADD;
  // state.colorSrcFactor = BLEND_F_SRC_ALPHA;
  // state.colorDstFactor = BLEND_F_DST_ALPHA;
  // state.alphaBlendOp = BLEND_OP_ADD;
  // state.alphaSrcFactor = BLEND_F_SRC_ALPHA;
  // state.alphaDstFactor = BLEND_F_DST_ALPHA;
  // state.cullMode = CULL_NONE;
  prog->setRenderState(state);

  return prog;
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/DeferredShading") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_VERTEX).setFromBinary(gDeferredShading_vs, sizeof(gDeferredShading_vs));
  prog->stage(SHADER_TYPE_FRAGMENT).setFromBinary(gDeferredShading_ps, sizeof(gDeferredShading_ps));
  prog->compile();

  // RenderState state;
  // state.isWriteDepth = FLAG_FALSE;
  // state.depthMode = COMPARE_ALWAYS;
  // state.depthMode = COMPARE_LEQUAL;
  // state.colorBlendOp = BLEND_OP_ADD;
  // state.colorSrcFactor = BLEND_F_SRC_ALPHA;
  // state.colorDstFactor = BLEND_F_DST_ALPHA;
  // state.alphaBlendOp = BLEND_OP_ADD;
  // state.alphaSrcFactor = BLEND_F_SRC_ALPHA;
  // state.alphaDstFactor = BLEND_F_DST_ALPHA;
  // state.cullMode = CULL_NONE;
  // prog->setRenderState(state);

  return prog;
}