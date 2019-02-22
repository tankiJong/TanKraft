#pragma once
#include "Engine/Core/common.hpp"
#include "Game/World/BlockDef.hpp"

class BlockDef;

class Block {
  friend class BlockDef;
public:
  static constexpr uint8_t kOpaqueFlag = 0x1;
  void reset(const BlockDef& def);
  block_id_t id() const { return mType; }
  const BlockDef& type() const;
  bool opaque() const { return mBitFlags & kOpaqueFlag; }
  static Block invalid;
protected:
  block_id_t mType = 0;
  uint8_t mBitFlags = kOpaqueFlag;
};
