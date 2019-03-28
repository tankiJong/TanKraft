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
#include "Engine/Renderer/RenderGraph/RenderNodeContext.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Debug/Draw.hpp"
#include "Engine/Gui/ImGui.hpp"

#include "VoxelRenderer/DeferredShading_ps.h"
#include "VoxelRenderer/DeferredShading_vs.h"
#include "VoxelRenderer/GenGBuffer_ps.h"
#include "VoxelRenderer/GenGBuffer_vs.h"
#include "VoxelRenderer/SSAO_cs.h"
#include "Engine/Renderer/RenderGraph/RenderPass/BlurPass.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/File/FileSystem.hpp"
#include "Game/World/World.hpp"
#include "Game/Utils/Config.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/RenderGraph/RenderNodeBuilder.hpp"

// DFS TODO: add ConstBuffer class66

void VoxelRenderer::onLoad(RHIContext&) {
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

  mTexAO = Texture2::create( width, height, TEXTURE_FORMAT_RGBA8, 
                            RHIResource::BindingFlag::RenderTarget | 
                                RHIResource::BindingFlag::UnorderedAccess | 
                                RHIResource::BindingFlag::ShaderResource);
  NAME_RHIRES(mTexAO);

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


  mFrameData.gViewDistance.x = Config::kMaxActivateDistance - 100;
  mFrameData.gViewDistance.y = Config::kMaxActivateDistance;
}

void VoxelRenderer::onRenderFrame(RHIContext& ctx) {
  
  updateFrameConstant(ctx);
  updateViewConstant(ctx);

  cleanBuffers(ctx);
  // mGraph.setCustomBackBuffer("color_result");
  // or
  // mGraph.setCustomBackBuffer(mTFinal);


  bool result = mGraph.execute();
  ENSURES(result);
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

void VoxelRenderer::onRenderGui(RHIContext&) {

}

void VoxelRenderer::issueChunk(const Chunk* chunk) {
  if(chunk->mesh() == nullptr) return;

  vec3 basePosition = chunk->coords().pivotPosition();
  mFrameRenderData.emplace_back(ChunkRenderData{chunk->mesh(), mat44::translation(basePosition), chunk});
}

void VoxelRenderer::raytracing(const Chunk* chunk) {
  
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
  ctx.clearRenderTarget(*mTexAO->rtv(), Rgba::white);

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

void VoxelRenderer::updateFrameConstant(RHIContext&) {
  mFrameData.gFrameCount++;
  mFrameData.gTime += (float)gGameClock->frame.second;
  mFrameData.gWorldConstant.x = mWorld->currentLightningStrikeLevel();
  mFrameData.gWorldConstant.y = mWorld->currentLightFlickLevel();
  static vec3  gRayleigh{5.5f, 13.0f, 22.4f};
  static vec3 gMie{21, 21, 21};
  static float  scaleRayleigh = 1e-6, scaleMie = 1e-6;

  float dayaPrecent = mFrameData.gTime / 86400.f;

  {
    ImGui::Begin("Constants");
      ImGui::DragFloat("Planet Radius", &mFrameData.gPlanetRadius, 100, Config::kMaxActivateDistance, 1e7);
      ImGui::DragFloat("Atmosphere Thickness", &mFrameData.gPlanetAtmosphereThickness, 10, 100, mFrameData.gPlanetRadius);
      ImGui::SliderFloat("Time", &dayaPrecent, 0, 1);
      ImGui::SliderFloat("Sun Power", &mFrameData.gSunPower, 1, 100, "%.1f");
      ImGui::DragFloat("Rayleigh factor scale", &scaleRayleigh, 0.0000001, 0.0000001, 1, "%.10f");
      ImGui::DragFloat3("Rayleigh Factor", (float*)&gRayleigh, 0.01, 0, 100, "%.3f");
      ImGui::DragFloat("Mie scale", &scaleMie, 0.000000001, 0, 1, "%.10f");
      ImGui::DragFloat3("Mie Factor", (float*)&gMie, 0.01, 0, 100, "%.3f");
      ImGui::DragFloat2("Scatter Thickness: Raylegigh/Mie", (float*)&mFrameData.gScatterThickness, 1, 10, 10000);
    ImGui::End();
  }
  mFrameData.gRayleigh = gRayleigh * scaleRayleigh;
  mFrameData.gMie = gMie * scaleMie;
  // mFrameData.gTime = .28 * 86400.f;
  mFrameData.gSunDir = vec3(0, sinf(dayaPrecent * 2 * PI - .5f * PI), cosf(dayaPrecent * 2 * PI - .5f * PI));
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
  // auto& genBufferPass = mGraph.defineNode("G-Buffer",[&](RenderNodeContext& builder) {
  //    
  //   S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/GenGBuffer");
  //   builder.reset(prog);
  //
  //   builder.readCbv("frame-data", mCFrameData, 0);
  //   builder.readCbv("camera-mat", mCCamera, 1);
  //   builder.readCbv("model-mat", mCModel, 2);
  //
  //   auto albedo = Resource<Texture2>::get("/Data/Images/Terrain_32x32.png");
  //
  //   builder.readSrv("albedo-tex", *albedo->srv(0, 6), 0);
  //
  //   builder.writeRtv("g-albedo", mGAlbedo, 0);
  //   builder.writeRtv("g-normal", mGNormal, 1);
  //   builder.writeRtv("g-tangent", mGTangent, 2);
  //   builder.writeRtv("g-bitangent", mGBiTangent, 3);
  //   builder.writeRtv("g-position", mGPosition, 4);
  //
  //   builder.writeDsv("g-depth", mGDepth);
  //
  //   return [&](RHIContext& ctx) {
  //     for(auto& renderData: mFrameRenderData) {
  //       mCModel->updateData(mat44::identity);
  //       // foreach( auto mesh: mScene.meshes() ) 
  //       renderData.mesh->bindForContext(ctx);
  //       for(const draw_instr_t& instr: renderData.mesh->instructions()) {
  //         ctx.setPrimitiveTopology(instr.prim);
  //         if(instr.useIndices) {
  //           ctx.drawIndexed(0, instr.startIndex, instr.elementCount);
  //         } else {
  //           ctx.draw(instr.startIndex, instr.elementCount);
  //         }
  //       }
  //     }
  //   };
  // });
  //
  // auto& ssaoPass = mGraph.defineNode("ssao-generate", [&](RenderNodeContext& builder) {
  //
  //   S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/SSAO_generate");
  //   builder.reset(prog, true);
  //   
  //   builder.readCbv("frame-data", mCFrameData, 0);
  //   builder.readCbv("camera-mat", mCCamera, 1);
  //   builder.readCbv("model-mat", mCModel, 2);
  //
  //   // builder.readSrv("g-albedo", mGAlbedo, 0);
  //   builder.readSrv("g-normal", mGNormal, 1);
  //   builder.readSrv("g-tangent", mGTangent, 2);
  //   builder.readSrv("g-bitangent", mGBiTangent, 3);
  //   builder.readSrv("g-position", mGPosition, 4);
  //   builder.readSrv("g-depth", mGDepth, 5);
  //
  //   builder.readWriteUav("g-texAO", mTexAO, 0);
  //
  //   return [&](RHIContext& ctx) {
  //     if(Input::Get().isKeyDown('N')) return;
  //     auto size = Window::Get()->bounds().size();
  //
  //     uint width = (uint)size.x;
  //     uint height = (uint)size.y;
  //     ctx.dispatch( width / 16 + 1, height / 16 + 1, 1);
  //   };
  // });
  //
  // auto& ssaoBlurPassV = mGraph.createNode<BlurPass>("ssao-blur-v", mTexAO, true);
  // auto& ssaoBlurPassH = mGraph.createNode<BlurPass>("ssao-blur-h", mTexAO, false);
  //
  // auto& deferredShadingPass = mGraph.defineNode("DeferredShading", [&](RenderNodeContext& builder) {
  //
  //   S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/DeferredShading");
  //   builder.reset(prog);
  //   
  //   builder.readCbv("frame-data", mCFrameData, 0);
  //   builder.readCbv("camera-mat", mCCamera, 1);
  //   builder.readCbv("model-mat", mCModel, 2);
  //   builder.writeRtv("final-image", mTFinal, 0);
  //
  //   auto skybox = Resource<TextureCube>::get("/Data/Images/skybox_texture.cube.jpg");
  //
  //   builder.readSrv("g-albedo", mGAlbedo, 0);
  //   builder.readSrv("g-normal", mGNormal, 1);
  //   builder.readSrv("g-tangent", mGTangent, 2);
  //   builder.readSrv("g-bitangent", mGBiTangent, 3);
  //   builder.readSrv("g-position", mGPosition, 4);
  //   builder.readSrv("g-texAO", mTexAO, 5);
  //   builder.readSrv("g-depth", mGDepth, 6);
  //   builder.readSrv("g-lights", mTLights, 7);
  //   builder.readSrv("tex-sky", skybox, 8);
  //
  //   return [&](RHIContext& ctx) {
  //     ctx.draw(0, 3);
  //     ctx.copyResource(*mGDepth, *RHIDevice::get()->depthBuffer());
  //   };
  // });
  //
  // auto& debugLightPass = mGraph.defineNode("Debug:Light", [&](RenderNodeContext& builder) {
  //   auto prog = Resource<Program>::get("internal/Shader/debug/always");
  //   builder.reset(prog);
  //
  //   RHIBuffer::sptr_t tintBuffer = RHIBuffer::create(sizeof(vec4), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write, &vec4::one);
  //
  //   builder.readCbv("camera-mat", mCCamera, 1);
  //   builder.readCbv("tint-buffer", tintBuffer, 6);
  //
  //   builder.writeRtv("final-image", mTFinal, 0);
  //
  //   return [&](RHIContext& ctx) {
  //     Mesh* mesh = mWorld->aquireDebugLightDirtyMesh();
  //     if(!mesh) return;
  //     mesh->bindForContext(ctx);
  //     for(const draw_instr_t& instr: mesh->instructions()) {
  //       ctx.setPrimitiveTopology(instr.prim);
  //       if(instr.useIndices) {
  //         ctx.drawIndexed(0, instr.startIndex, instr.elementCount);
  //       } else {
  //         ctx.draw(instr.startIndex, instr.elementCount);
  //       }
  //     }
  //
  //     SAFE_DELETE(mesh);
  //   };
  // });
  // // this is not optional, consider case like: a->pass1->a, a->pass2->a, a->pass2->a, you cannot figure out the sequence.
  // // mGraph.depend(genBufferPass, ssaoPass);
  // // mGraph.depend(genBufferPass, deferredShadingPass);
  // mGraph.depend(ssaoBlurPassV, ssaoBlurPassH);
  // mGraph.depend(ssaoBlurPassH, deferredShadingPass);
  // mGraph.depend(deferredShadingPass, debugLightPass);
  //
  // // another way to go with is specify dependencies among resources, which gives the freedom to name res differently among nodes.
  // mGraph.connect(genBufferPass, "g-albedo", deferredShadingPass, "g-albedo");
  // mGraph.connect(genBufferPass, "g-normal", deferredShadingPass, "g-normal");
  // mGraph.connect(genBufferPass, "g-tangent", deferredShadingPass, "g-tangent");
  // mGraph.connect(genBufferPass, "g-bitangent", deferredShadingPass, "g-bitangent");
  // mGraph.connect(genBufferPass, "g-position", deferredShadingPass, "g-position");
  // mGraph.connect(genBufferPass, "g-depth", deferredShadingPass, "g-depth");
  //
  // mGraph.connect(genBufferPass, "g-normal", ssaoPass, "g-normal");
  // mGraph.connect(genBufferPass, "g-tangent", ssaoPass, "g-tangent");
  // mGraph.connect(genBufferPass, "g-bitangent", ssaoPass, "g-bitangent");
  // mGraph.connect(genBufferPass, "g-position", ssaoPass, "g-position");
  // mGraph.connect(genBufferPass, "g-depth", ssaoPass, "g-depth");
  //
  // mGraph.connect(ssaoPass, "g-texAO", ssaoBlurPassV, "blur-input");
  //
  // /* I will want something like: genBufferPass["g-albedo"] >> deferredShadingPass["g-albedo"]
  // / or: mGraph | "passA.gAlbedo" >> "passB.color"
  //              | "passA.gNormal" >> "passB.worldNormal"
  // */
  //
  // // mGraph.setOutput(ssaoPass, "g-texAO");
  // // mGraph.setOutput(ssaoBlurPassH, "blur-output");
  // // mGraph.setOutput(deferredShadingPass, "final-image");
  // mGraph.setOutput(debugLightPass, "final-image");

  // mGraph.declare<TextureCube>("skybox");
  mGraph.declare<Texture2>("ao_buffer");
  mGraph.declare<const Texture2>("albedo");

  mGraph.declare<RHIBuffer>("frameConstant");
  mGraph.declare<RHIBuffer>("cameraInfo");   
  mGraph.declare<RHIBuffer>("modelMatrix");

  mGraph.declare<std::vector<ChunkRenderData>>("RenderData");

  auto& genBufferPass = mGraph.defineNode("G-Buffer", [](RenderNodeBuilder& builder, RenderNodeContext& context) {

      RenderGraphResourceDesc desc;
      desc.type = RHIResource::Type::Texture2D;
      desc.texture2.size = uvec2{Window::Get()->bounds().size()};
      desc.bindingFlags = RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::ShaderResource;

      desc.texture2.format = TEXTURE_FORMAT_RGBA8;
      builder.output<Texture2>("out_albedo"   , desc);

      desc.texture2.format = TEXTURE_FORMAT_RGBA16;
      builder.output<Texture2>("out_normal"   , desc);
      builder.output<Texture2>("out_tangent"  , desc);
      builder.output<Texture2>("out_bitangent", desc);
      builder.output<Texture2>("out_position" , desc);

      desc.texture2.format = TEXTURE_FORMAT_D24S8;
      desc.bindingFlags = RHIResource::BindingFlag::DepthStencil | RHIResource::BindingFlag::ShaderResource;
      builder.output<Texture2>("out_depth"    , desc);

    {
      S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/GenGBuffer");
      context.reset(prog);

      context.readCbv("frameConstant", 0);
      context.readCbv("cameraInfo",    1);
      context.readCbv("modelMatrix",   2);

      context.readSrv("albedo",        0);

      context.writeRtv(".out_albedo",    0);
      context.writeRtv(".out_normal",    1);
      context.writeRtv(".out_tangent",   2);
      context.writeRtv(".out_bitangent", 3);
      context.writeRtv(".out_position",  4);

      context.writeDsv(".out_depth");
    }

    return [=](const RenderGraphResourceSet& set, RHIContext& ctx) {
      const std::vector<ChunkRenderData>*    frameRenderData =
          set.get<std::vector<ChunkRenderData>>("RenderData");
      // set.get<std::vector<ChunkRenderData>>("RenderData");

      Texture2::sptr_t albedo    = set.get<Texture2>("G-Buffer.out_albedo");
      Texture2::sptr_t normal    = set.get<Texture2>("G-Buffer.out_normal");
      Texture2::sptr_t tangent   = set.get<Texture2>("G-Buffer.out_tangent");
      Texture2::sptr_t bitangent = set.get<Texture2>("G-Buffer.out_bitangent");
      Texture2::sptr_t depth     = set.get<Texture2>("G-Buffer.out_depth");

      ctx.clearRenderTarget(*albedo->rtv(), Rgba::black);
      ctx.clearRenderTarget(*tangent->rtv(), Rgba::gray);
      ctx.clearRenderTarget(*bitangent->rtv(), Rgba::gray);
      ctx.clearRenderTarget(*normal->rtv(), Rgba::gray);
      ctx.clearDepthStencilTarget(*depth->dsv());

      for(auto& renderData: *frameRenderData) {

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

  auto& ssaoPass = mGraph.defineNode("ssao-generate", 
    [](RenderNodeBuilder& builder, RenderNodeContext& context) {
    
    builder.input<Texture2> ("normal"   );
    builder.input<Texture2> ("tangent"  );
    builder.input<Texture2> ("bitangent");
    builder.input<Texture2> ("position" );
    builder.input<Texture2> ("depth"    );

    RenderGraphResourceDesc desc;
    desc.type = RHIResource::Type::Texture2D;
    desc.texture2.format = TEXTURE_FORMAT_RGBA8;
    builder.output<Texture2>("out_ao", desc);

    {
      S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/SSAO_generate");
      context.reset(prog, true);
      
      context.readCbv("frameConstant", 0);
      context.readCbv("cameraInfo",    1);

      // builder.readSrv("g-albedo", mGAlbedo, 0);
      context.readSrv(".normal",    1);
      context.readSrv(".tangent",   2);
      context.readSrv(".bitangent", 3);
      context.readSrv(".position",  4);
      context.readSrv(".depth",     5);

      context.readWriteUav(".out_ao", 0);
    }

    return [&](const RenderGraphResourceSet&, RHIContext& ctx) {
      if(Input::Get().isKeyDown('N')) return;
      auto size = Window::Get()->bounds().size();

      uint width = (uint)size.x;
      uint height = (uint)size.y;
      ctx.dispatch( width / 8 + 1, height / 8 + 1, 1);
    };

  });

  auto& blurpass = mGraph.defineNode("Blur", 
  [](RenderNodeBuilder& builder, RenderNodeContext& context) {  

	  auto& target = builder.inputOutput<Texture2>("target");

	  // --------------------
    // this part could be skipped with more infrastructure in the shader program system
		  S<const Program> prog = Resource<Program>::get("internal/ShaderPass/Blur");
		  context.reset(prog, true);

		  // get from transient resource system
		  // get from certain pass output
		  context.readSrv(".target", 0, 0);

		  // I am asking a temporary resource to render
      RenderGraphResourceDesc desc;
      desc.texture2.format = TEXTURE_FORMAT_RGBA8;
		  auto& tex = context.extend(target, "temp_buffer", desc);
		  
		  context.writeRtv(tex, 0);
	  // ---------------------


	  return [&](const RenderGraphResourceSet& set, RHIContext& ctx) {
		  ctx.draw(0, 3);

		  Texture2::sptr_t from = set.get<Texture2>(tex);

		  // specify current node, then get something from this node by name
		  Texture2::sptr_t to   = set.get<Texture2>("Blur.result"); 

		  ctx.copyResource(*from, *to);
	  };
  });

  auto& ssaoBlurPassV = mGraph.createNode<BlurPass>("ssao-blur-v", true);
  auto& ssaoBlurPassH = mGraph.createNode<BlurPass>("ssao-blur-h", false);

  auto& deferredShadingPass = mGraph.defineNode("DeferredShading",
  [](RenderNodeBuilder& builder, RenderNodeContext& context) {

    builder.input<Texture2>("gAlbedo"   );
    builder.input<Texture2>("gNormal"   );
    builder.input<Texture2>("gTangent"  );
    builder.input<Texture2>("gBitangent");
    builder.input<Texture2>("gPosition" );
    builder.input<Texture2>("gAO"       );
    builder.input<Texture2>("gDepth"    );
    // builder.input("skybox"        RHIResource::Type::TextureCube);
    // builder.input<RHIBuffer>("lightBuffer");

    RenderGraphResourceDesc desc;
    desc.type = RHIResource::Type::Texture2D;
    desc.texture2.size = uvec2{Window::Get()->bounds().size()};
    desc.bindingFlags = RHIResource::BindingFlag::RenderTarget;
    desc.texture2.format = TEXTURE_FORMAT_RGBA8;

    builder.output<Texture2>("out_finalImage", desc);
    {
      S<const Program> prog = Resource<Program>::get("Game/Shader/Voxel/DeferredShading");
      context.reset(prog);
      
      context.readCbv("frameConstant"    , 0);
      context.readCbv("cameraInfo"       , 1);

      context.readSrv(".gAlbedo"          , 0);
      context.readSrv(".gNormal"         , 1);
      context.readSrv(".gTangent"        , 2);
      context.readSrv(".gBitangent"       , 3);
      context.readSrv(".gPosition"        , 4);
      context.readSrv(".gAO"              , 5);
      context.readSrv(".gDepth"           , 6);
      // context.readSrv(".lightBuffer"       , 7);
      // context.readSrv("skybox"            , 8);

      context.writeRtv(".out_finalImage" , 0);
    }


    return [=](const RenderGraphResourceSet& set, RHIContext& ctx) {
      ctx.draw(0, 3);

      const Texture2::sptr_t depth = set.get<Texture2>("DeferredShading.gDepth");
      ctx.copyResource(*depth, *RHIDevice::get()->depthBuffer());

      Texture2::sptr_t backBuffer = set.backBuffer();
		  Texture2::sptr_t from = set.get<Texture2>("DeferredShading.out_finalImage");
		  ctx.copyResource(*from, *backBuffer);
    };
  });

  mGraph.connect(genBufferPass, "out_normal",   ssaoPass, "normal");
  mGraph.connect(genBufferPass, "out_tangent",  ssaoPass, "tangent");
  mGraph.connect(genBufferPass, "out_bitangent",ssaoPass, "bitangent");
  mGraph.connect(genBufferPass, "out_position", ssaoPass, "position");
  mGraph.connect(genBufferPass, "out_depth",    ssaoPass, "depth");

  mGraph.connect(ssaoPass,      "out_ao",       ssaoBlurPassV, "target");
  mGraph.connect(ssaoBlurPassV, "target",   ssaoBlurPassH, "target");

  mGraph.connect(genBufferPass, "out_albedo",   deferredShadingPass, "gAlbedo");
  mGraph.connect(genBufferPass, "out_normal",   deferredShadingPass, "gNormal");
  mGraph.connect(genBufferPass, "out_tangent",  deferredShadingPass, "gTangent");
  mGraph.connect(genBufferPass, "out_bitangent",deferredShadingPass, "gBitangent");
  mGraph.connect(genBufferPass, "out_position", deferredShadingPass, "gPosition");
  mGraph.connect(genBufferPass, "out_depth",    deferredShadingPass, "gDepth");
  mGraph.connect(ssaoBlurPassH, "target",   deferredShadingPass, "gAO");
  // mGraph.connect(genBufferPass, "out_depth",    deferredShadingPass, "lightBuffer");
  
  mGraph.bind("ssao-generate.out_ao", "ao_buffer");
  
  mGraph.build();

  mGraph.setOutputPass(deferredShadingPass);
  // mGraph.setParam(mTFinal, "color_result");

  mGraph.setParam<Texture2>(mTexAO, "ao_buffer");

  auto albedo = Resource<Texture2>::get("/Data/Images/Terrain_32x32.png");
  mGraph.setParam<const Texture2>(albedo, "albedo");

  mGraph.setParam<RHIBuffer>(mCFrameData, "frameConstant");
  mGraph.setParam<RHIBuffer>(mCCamera, "cameraInfo");   
  mGraph.setParam<RHIBuffer>(mCModel, "modelMatrix");
  mGraph.setParam<std::vector<ChunkRenderData>>(&mFrameRenderData, "RenderData");

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

  return prog;
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/SSAO_generate") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_COMPUTE).setFromBinary(gSSAO_cs, sizeof(gSSAO_cs));
  prog->compile();

  return prog;
}
//http://blog.tuxedolabs.com/2018/10/17/from-screen-space-to-voxel-space.html