#include "GPUVolume.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Game/World/World.hpp"
#include "Engine/Graphics/Program/Program.hpp"
#include "World/VolumeUpdate_cs.h"
#include "Engine/Graphics/Program/ProgramInst.hpp"
#include "Engine/Graphics/RHI/PipelineState.hpp"
#include "World/GenVolumeMipmap_Detail_cs.h"
#include "World/GenVolumeMipmap_Rough_cs.h"
void GPUVolume::init(uint tileCountX, uint tileCountY, eTextureFormat format) {
  mVolume = Texture3::create(
	  tileCountX * kTileSizeX, tileCountY * kTileSizeY, 1 * kTileSizeZ, format,
	  RHIResource::BindingFlag::ShaderResource | RHIResource::BindingFlag::UnorderedAccess);
	NAME_RHIRES(mVolume);

	mVisibilityVolume = Texture3::create(
	  tileCountX * kTileSizeX, tileCountY * kTileSizeY, 1 * kTileSizeZ, TEXTURE_FORMAT_R8_UINT, true,
	  RHIResource::BindingFlag::ShaderResource | RHIResource::BindingFlag::UnorderedAccess);
	NAME_RHIRES(mVisibilityVolume);

	mTileCountX = tileCountX;
	mTileCountY = tileCountY;
  mPlayerTileIndex = { kTileSizeX >> 1, kTileSizeY >> 1 }; // player is alway at the center
}

void GPUVolume::update(vec3 playerPosition) {
	static auto prog = Resource<Program>::get("Game/Shader/Voxel/VolumeUpdate");
	ProgramInst::sptr_t inst = ComputeProgramInst::create(prog);

	static ComputeState::sptr_t cs;

	if(cs == nullptr) {
		ComputeState::Desc desc;
		desc.setProgram(prog);
		desc.setRootSignature(prog->rootSignature());
		cs = ComputeState::create(desc);
	}

	auto ctx = RHIDevice::get()->defaultRenderContext();
	ctx->bindDescriptorHeap();

	mPlayerPosition = playerPosition;

	Chunk* centerChunk = mWorld->findChunk(playerPosition);
	if(!centerChunk->valid()) return;
	uvec3 volumeSize;
	{
		const Texture3::sptr_t& chunkVolume = centerChunk->gpuVolume();
		volumeSize = { chunkVolume->width(), chunkVolume->height(), chunkVolume->depth() };
		ctx->transitionBarrier(chunkVolume.get(), RHIResource::State::ShaderResource);
		inst->setSrv(*chunkVolume->srv(0, 1), mPlayerTileIndex.x + mTileCountX * mPlayerTileIndex.y);
		//RHIDevice::get()
		//				->defaultRenderContext()
		//				->copyTextureRegion(
		//					mVolume.get(), 0, voxelIndexOffset(mPlayerTileIndex), 
		//					chunkVolume.get(), 0, {0, 0, 0}, volumeSize);
	}

	std::vector<Chunk::Iterator> iterX;
	iterX.resize(mTileCountX);

	// -x
	{
		uint d = 1;
		Chunk::Iterator next = centerChunk->iterator();
		iterX[mPlayerTileIndex.x] = next;
		while(mPlayerTileIndex.x - d < mPlayerTileIndex.x) {
			next.step(Chunk::NEIGHBOR_NEG_X);
			iterX[mPlayerTileIndex.x - d] = next;
			d++;
		}
	}

	// +x
	{
		uint d = 1;
		Chunk::Iterator next = centerChunk->iterator();
		iterX[mPlayerTileIndex.x] = next;
		while(mPlayerTileIndex.x + d < mTileCountX) {
			next.step(Chunk::NEIGHBOR_POS_X);
			iterX[mPlayerTileIndex.x + d] = next;
			d++;
		}
	}

	for(size_t i = 0; i < iterX.size(); i++) {
		// -y, 0
		Chunk::Iterator iter = iterX[i];
		if(!iter->valid()) continue;
		{
			uint d = 0;
			Chunk::Iterator next = iter;
			while(mPlayerTileIndex.y - d <= mPlayerTileIndex.y) {
				next.step(Chunk::NEIGHBOR_NEG_Y);
				ctx->transitionBarrier(next->gpuVolume().get(), RHIResource::State::ShaderResource);
				inst->setSrv(*next->gpuVolume()->srv(0, 1), i + mTileCountX * (mPlayerTileIndex.y - d));

				//if(next.valid() && currentCoords != next->coords()) {
				//	currentCoords = next->coords();
				//	RHIDevice::get()
				//				->defaultRenderContext()
				//				->copyTextureRegion(
				//					mVolume.get(), 0, voxelIndexOffset(i, mPlayerTileIndex.y - d), 
				//					next->gpuVolume().get(), 0, {0, 0, 0}, volumeSize);
				//}
				d++;
			}
		}
		// +y
		{
			uint d = 1;
			Chunk::Iterator next = iter;
			while(mPlayerTileIndex.y + d < mTileCountY) {
				next.step(Chunk::NEIGHBOR_POS_Y);
				ctx->transitionBarrier(next->gpuVolume().get(), RHIResource::State::ShaderResource);
				inst->setSrv(*next->gpuVolume()->srv(0, 1), i + mTileCountX * (mPlayerTileIndex.y + d));
				//if(next.valid() && currentCoords != next->coords()) {
				//	currentCoords = next->coords();
				//	RHIDevice::get()
				//				->defaultRenderContext()
				//				->copyTextureRegion(
				//					mVolume.get(), 0, voxelIndexOffset(i, mPlayerTileIndex.y + d), 
				//					next->gpuVolume().get(), 0, {0, 0, 0}, volumeSize);
				//}
				d++;
			}
		}

	}
	ctx->transitionBarrier(mVolume.get(), RHIResource::State::UnorderedAccess);

	inst->setUav(*mVolume->uav(0), 0);

	ctx->setComputeState(*cs);
	inst->apply(*ctx, false);

	ctx->dispatch(16, 16, 1);

	updateSubVolumeAndMipmapsDetail();


	updateSubVolumeAndMipmapsRough();
}

uvec3 GPUVolume::voxelIndexOffset(uint tileIndexX, uint tileIndexY) {
	if( kTileSizeX * tileIndexX >= 256 || kTileSizeY * tileIndexY >= 256) {
		BAD_CODE_PATH();
	}
	return { kTileSizeX * tileIndexX, kTileSizeY * tileIndexY, 0 };
}

void GPUVolume::updateSubVolumeAndMipmapsDetail() const {
	static auto prog = Resource<Program>::get("Game/Shader/Voxel/GenVolumeMipmap_Detail");
	ProgramInst::sptr_t inst = ComputeProgramInst::create(prog);

	static ComputeState::sptr_t cs;

	if(cs == nullptr) {
		ComputeState::Desc desc;
		desc.setProgram(prog);
		desc.setRootSignature(prog->rootSignature());
		cs = ComputeState::create(desc);
	}

	auto ctx = RHIDevice::get()->defaultRenderContext();
	
	ctx->transitionBarrier(mVolume.get(), RHIResource::State::ShaderResource);
	ctx->transitionBarrier(mVisibilityVolume.get(), RHIResource::State::UnorderedAccess);
	
	inst->setSrv(*mVolume->srv(0), 0);
	for(uint i = 0; i < 3; i++) {
		inst->setUav(*mVisibilityVolume->uav(i), i, 0);
	}

	ctx->setComputeState(*cs);
	inst->apply(*ctx, false);

	ctx->dispatch(32, 32, 32);
}

void GPUVolume::updateSubVolumeAndMipmapsRough() const {
	static auto prog = Resource<Program>::get("Game/Shader/Voxel/GenVolumeMipmap_Rough");

	static ComputeState::sptr_t cs;

	if(cs == nullptr) {
		ComputeState::Desc desc;
		desc.setProgram(prog);
		desc.setRootSignature(prog->rootSignature());
		cs = ComputeState::create(desc);
	}

	auto ctx = RHIDevice::get()->defaultRenderContext();
	

	ctx->setComputeState(*cs);

	uint startMip = 2;
	for(uint dim = 32; dim > 0; dim = dim >> 1){
		ProgramInst::sptr_t inst = ComputeProgramInst::create(prog);

		inst->setUav(*mVisibilityVolume->uav(startMip), 0, 0);
		inst->setUav(*mVisibilityVolume->uav(++startMip), 1, 0);

		inst->apply(*ctx, false);
		ctx->uavBarrier(mVisibilityVolume.get());
		ctx->dispatch(dim, dim, dim);
	}
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/VolumeUpdate") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_COMPUTE).setFromBinary(gVolumeUpdate_cs, sizeof(gVolumeUpdate_cs));
  prog->compile();

  return prog;
}



DEF_RESOURCE(Program, "Game/Shader/Voxel/GenVolumeMipmap_Detail") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_COMPUTE).setFromBinary(gGenVolumeMipmap_Detail_cs, sizeof(gGenVolumeMipmap_Detail_cs));
  prog->compile();

  return prog;
}

DEF_RESOURCE(Program, "Game/Shader/Voxel/GenVolumeMipmap_Rough") {
  Program::sptr_t prog = Program::sptr_t(new Program());

  prog->stage(SHADER_TYPE_COMPUTE).setFromBinary(gGenVolumeMipmap_Rough_cs, sizeof(gGenVolumeMipmap_Rough_cs));
  prog->compile();

  return prog;
}