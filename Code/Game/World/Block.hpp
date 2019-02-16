#pragma once
#include "Engine/Core/common.hpp"
#include "Game/World/BlockDef.hpp"

class BlockDef;

class Block {
public:
  void reset(const BlockDef& def);
  block_id_t id() const { return mType; }
  const BlockDef& type() const;
  bool opaque() const { return type().opaque(); }
  static Block invalid;
protected:
  block_id_t mType = 0;
  uint8_t mState = 0;
};
