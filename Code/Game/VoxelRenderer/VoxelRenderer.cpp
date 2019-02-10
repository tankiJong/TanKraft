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
#include "Engine/Renderer/RenderGraph/RenderNodeContext.hpp"
#include "Engine/Renderer/Shader/ShaderInterop.h"

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

  mGTangent = Texture2::create( width, height, TEXTURE_FORMAT_RGBA16, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGTangent);

  mGBiTangent = Texture2::create( width, height, TEXTURE_FORMAT_RGBA16, 
                 RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGBiTangent);

  mGDepth = Texture2::create( width, height, TEXTURE_FORMAT_D24S8, 
                 RHIResource::BindingFlag::DepthStencil | RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mGDepth);

  mCFrameData = RHIBuffer::create(sizeof(frame_data_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCFrameData);

  mCCamera = RHIBuffer::create(sizeof(camera_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCCamera);

  mCModel = RHIBuffer::create(sizeof(mat44), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write);
  NAME_RHIRES(mCModel);

  
  mTLights = TypedBuffer::For<light_info_t>(400, RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mTLights);

  // constructFrameMesh();
  defineRenderPasses();

}

void VoxelRenderer::onRenderFrame(RHIContext& ctx) {
  
  // constructTestSphere();
  
  updateFrameConstant(ctx);
  updateViewConstant(ctx);

  cleanBuffers(ctx);
  bool result = mGraph.execute();

  cleanupFrameData();
  //
  // ctx.bindDescriptorHeap();
  //
  // //---------
  // ctx.transitionBarrier(mGAlbedo.get(), RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGPosition.get(), RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGNormal.get(), RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGTangent.get(), RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGBiTangent.get(), RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGDepth.get(), RHIResource::State::DepthStencil);
  //
  // //---------
  // generateGbuffer(ctx);
  //
  // //---------
  // ctx.transitionBarrier(mTFinal.get(), 
  //                       RHIResource::State::RenderTarget);
  // ctx.transitionBarrier(mGAlbedo.get(), RHIResource::State::ShaderResource);
  // ctx.transitionBarrier(mGPosition.get(), RHIResource::State::ShaderResource);
  // ctx.transitionBarrier(mGNormal.get(), RHIResource::State::ShaderResource);
  // ctx.transitionBarrier(mGTangent.get(), RHIResource::State::ShaderResource);
  // ctx.transitionBarrier(mGBiTangent.get(), RHIResource::State::ShaderResource);
  // ctx.transitionBarrier(mGDepth.get(), RHIResource::State::ShaderResource);
  // deferredShading(ctx);
  //
  //
  // //---------
  // ctx.transitionBarrier(mTFinal.get(), RHIResource::State::CopySource);
  // ctx.transitionBarrier(RHIDevice::get()->backBuffer().get(), RHIResource::State::CopyDest);
  // copyToBackBuffer(ctx);

}

void VoxelRenderer::issueChunk(const Chunk* chunk) {
  if(chunk->mesh() == nullptr) return;

  vec3 basePosition = chunk->coords().pivotPosition();
  mFrameRenderData.emplace_back(ChunckRenderData{chunk->mesh(), mat44::translation(basePosition)});
}

VoxelRenderer::~VoxelRenderer() {
  SAFE_DELETE(mFrameMesh);
}

void VoxelRenderer::cleanBuffers(RHIContext& ctx) {
  ctx.clearRenderTarget(*mGAlbedo->rtv(), Rgba::black);
  ctx.clearRenderTarget(*mGPosition->rtv(), Rgba::black);
  ctx.clearRenderTarget(*mGTangent->rtv(), Rgba::gray);
  ctx.clearRenderTarget(*mGBiTangent->rtv(), Rgba::gray);
  ctx.clearRenderTarget(*mGNormal->rtv(), Rgba::gray);

  ctx.clearRenderTarget(*mTFinal->rtv(), Rgba::black);
  
  ctx.clearDepthStencilTarget(*mGDepth->dsv());
}

void VoxelRenderer::generateGbuffer(RHIContext& ctx) {
  SCOPED_GPU_EVENT(ctx, "Gen G-Buffer");
  BAD_CODE_PATH();
  FrameBuffer fbo;

  fbo.defineColorTarget(mGAlbedo, 0);
  fbo.defineColorTarget(mGNormal, 1);
  fbo.defineColorTarget(mGTangent, 2);
  fbo.defineColorTarget(mGBiTangent, 3);
  fbo.defineColorTarget(mGPosition, 4);
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
    inst->setSrv(*albedo->srv(), 0);
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

    inst->setSrv(*mGAlbedo->srv(), 0);
    inst->setSrv(*mGNormal->srv(), 1);
    inst->setSrv(*mGTangent->srv(), 2);
    inst->setSrv(*mGBiTangent->srv(), 3);
    inst->setSrv(*mGPosition->srv(), 4);
    inst->setSrv(*mTLights->srv(), 5);
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

  mTLights = TypedBuffer::For<light_info_t>(200, RHIResource::BindingFlag::ShaderResource);
  
  Mesher ms;
  ms.begin(DRAW_TRIANGES);

  auto addBlock = [&](const vec3& center, const vec3& dimension) {
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

  for(int j = 0; j < 100; ++j) {
    for(int i = 0; i < 100; ++i) {
      int kmax = abs(Compute2dPerlinNoise(i, j, 50, 3)) * 5.f + 1;
      for(int k = 0; k < kmax; k++) {
        vec2 centerPosition { i * kBlockSize, j * kBlockSize };
        // ms.cube({centerPosition, k * kBlockSize}, vec3::one);
        addBlock(
          vec3{ centerPosition.x, centerPosition.y, k * kBlockSize }, 
          vec3{ kBlockSize });
      }
    }
  }

  ms.end();
  mFrameMesh = ms.createMesh();

  uint kk = 0;
  for(int j = 0; j < 20; ++j) {
    for(int i = 0; i < 20; ++i) {
      if(i % 2 != 0 || j % 2 != 0) continue;
      vec2 centerPosition { i * 1.f, j * 1.f };

      light_info_t light;

      vec3 lightPosition = {centerPosition.x, getRandomf01() * 5.f + 5.f, centerPosition.y};
      Rgba lightColor = { vec3{0, getRandomf01(), getRandomf01()} };
      light.asPointLight(
        lightPosition,
        100.f, vec3{0,0,1}, 
        lightColor);
      mTLights->set(kk++, light);

      // mat44 view = mCamera->view();
      // mat44 proj = mCamera->projection();
      // mat44 trans = mat44::makeTranslation(lightPosition);
      // ImGuizmo::DrawCube((float*)&view, (float*)&proj, (float*)&trans);
    }
  }
  mTLights->uploadGpu();

}

void VoxelRenderer::constructTestSphere() {
  static Transform lightTransform;
  static vec3 lightColor = vec3::one;
  static float intensity = 1.f;
  static vec3 meshColor;
  ImGui::gizmos(*mCamera, lightTransform, ImGuizmo::TRANSLATE);
  ImGui::Begin("Test Scene Control");
  ImGui::ColorEdit3("light color", (float*)&lightColor);
  ImGui::SliderFloat("light intensity", &intensity, 0.1f, 100.f);
  ImGui::SliderFloat3("light position", (float*)&lightTransform.localPosition(), 0.1f, 100.f);
  ImGui::ColorEdit3("mesh color", (float*)&meshColor);
  ImGui::SliderFloat2("Roughness, Metallic", &mFrameData.gRoughness, 0, 1);
  ImGui::End();

  delete mFrameMesh;
  Mesher ms;
  ms.begin(DRAW_TRIANGES);
  ms.color(vec4{meshColor, 1.f});
  ms.cube(vec3::zero, vec3::one);
  // ms.obj("/data/model/sphere.obj");
  ms.sphere(vec3::one, .5f, 50, 50);
  ms.end();
  mFrameMesh = ms.createMesh();


  light_info_t info;
  info.asPointLight(lightTransform.position(), intensity, {0,0,1}, lightColor);
  mTLights->set(0, info);
  mTLights->uploadGpu();
}

void VoxelRenderer::updateFrameConstant(RHIContext& ctx) {
  mFrameData.gFrameCount++;
  mFrameData.gTime = GetMainClock().total.second;
  mCFrameData->updateData(mFrameData);

  // mGDepth = RHIDevice::get()->depthBuffer();
}

void VoxelRenderer::updateViewConstant(RHIContext& ctx) {
  mCCamera->updateData(mCamera->ubo());
  aabb2 bounds = { vec2::zero, { Window::Get()->bounds().width(), Window::Get()->bounds().height()} };
  ctx.setViewport(bounds);
  ctx.setScissorRect(bounds);
}

void VoxelRenderer::defineRenderPasses() {
  auto& genBufferPass = mGraph.defineNode("G-Buffer",[&](RenderNodeContext& builder) {
     
    S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/GenGBuffer");
    builder.reset(prog);

    builder.readCbv("frame-data", mCFrameData, 0);
    builder.readCbv("camera-mat", mCCamera, 1);
    builder.readCbv("model-mat", mCModel, 2);

    auto albedo = Resource<Texture2>::get("/Data/Images/Terrain_32x32.png");

    builder.readSrv("albedo-tex", albedo, 0);

    builder.writeRtv("g-albedo", mGAlbedo, 0);
    builder.writeRtv("g-normal", mGNormal, 1);
    builder.writeRtv("g-tangent", mGTangent, 2);
    builder.writeRtv("g-bitangent", mGBiTangent, 3);
    builder.writeRtv("g-position", mGPosition, 4);

    builder.writeDsv("g-depth", mGDepth);

    return [&](RHIContext& ctx) {
      for(auto& renderData: mFrameRenderData) {
        mCModel->updateData(mat44::identity);
        // foreach( auto mesh: mScene.meshes() ) 
        renderData.mesh->bindForContext(ctx);
        for(const draw_instr_t& instr: renderData.mesh->instructions()) {
          ctx.setPrimitiveTopology(instr.prim);
          if(instr.useIndices) {
            ctx.drawIndexed(0, instr.startIndex, instr.elementCount);
          } else {
            ctx.draw(instr.startIndex, instr.elementCount);
          }
        }
      }
    };
  });

  auto& deferredShadingPass = mGraph.defineNode("DeferredShading", [&](RenderNodeContext& builder) {

    S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/DeferredShading");
    builder.reset(prog);
    
    builder.readCbv("frame-data", mCFrameData, 0);
    builder.readCbv("camera-mat", mCCamera, 1);
    builder.readCbv("model-mat", mCModel, 2);
    builder.writeRtv("final-image", mTFinal, 0);

    builder.readSrv("g-albedo", mGAlbedo, 0);
    builder.readSrv("g-normal", mGNormal, 1);
    builder.readSrv("g-tangent", mGTangent, 2);
    builder.readSrv("g-bitangent", mGBiTangent, 3);
    builder.readSrv("g-position", mGPosition, 4);
    builder.readSrv("g-lights", mTLights, 5);


    return [&](RHIContext& ctx) {
      ctx.draw(0, 3);
    };
  });

  // this is not optional, consider case like: a->pass1->a, a->pass2->a, a->pass2->a, you cannot figure out the sequence.
  mGraph.depend(genBufferPass, deferredShadingPass);

  // another way to go with is specify dependencies among resources, which gives the freedom to name res differently among nodes.
  mGraph.connect(genBufferPass, "g-albedo", deferredShadingPass, "g-albedo");
  mGraph.connect(genBufferPass, "g-normal", deferredShadingPass, "g-normal");
  mGraph.connect(genBufferPass, "g-tangent", deferredShadingPass, "g-tangent");
  mGraph.connect(genBufferPass, "g-bitangent", deferredShadingPass, "g-bitangent");
  mGraph.connect(genBufferPass, "g-position", deferredShadingPass, "g-position");
  /* I will want something like: genBufferPass["g-albedo"] >> deferredShadingPass["g-albedo"]
  / or: mGraph | "passA.gAlbedo" >> "passB.color"
               | "passA.gNormal" >> "passB.worldNormal"
  */

  mGraph.setOutput(deferredShadingPass, "final-image");

  mGraph.compile();

}

void VoxelRenderer::cleanupFrameData() {
  mFrameRenderData.clear();
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/GenGBuffer") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_VERTEX).setFromBinary(gGenGBuffer_vs, sizeof(gGenGBuffer_vs));
  prog->stage(SHADER_TYPE_FRAGMENT).setFromBinary(gGenGBuffer_ps, sizeof(gGenGBuffer_ps));
  prog->compile();

  RenderState state;
  // state.isWriteDepth = FLAG_FALSE;
  // state.depthMode = COMPARE_ALWAYS;
  // // state.depthMode = COMPARE_LEQUAL;
  // state.colorBlendOp = BLEND_OP_ADD;
  // state.colorSrcFactor = BLEND_F_SRC_ALPHA;
  // state.colorDstFactor = BLEND_F_DST_ALPHA;
  // state.alphaBlendOp = BLEND_OP_ADD;
  // state.alphaSrcFactor = BLEND_F_SRC_ALPHA;
  // state.alphaDstFactor = BLEND_F_DST_ALPHA;
  state.cullMode = CULL_BACK;
  // state.frontFace = WIND_CLOCKWISE;
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