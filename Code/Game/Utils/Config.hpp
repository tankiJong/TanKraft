#pragma once
#include "Engine/Core/common.hpp"

struct Config {
  static float kMaxActivateDistance;
  static float kMinDeactivateDistance;
  static uint kMaxChunkActivatePerFrame;
  static uint kMaxChunkDeactivatePerFrame;
  static uint kMaxChunkReconstructMeshPerFrame;
  static float kWorldTimeScale;
};