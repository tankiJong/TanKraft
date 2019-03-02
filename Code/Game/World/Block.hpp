#pragma once
#include "Engine/Core/common.hpp"
#include "Game/World/BlockDef.hpp"

class BlockDef;

class Block {
  friend class BlockDef;
  friend class Chunk;
public:
  static constexpr uint8_t kOpaqueFlag = BIT_FLAG(0);
  static constexpr uint8_t kDirtyFlag  = BIT_FLAG(1);

  static constexpr uint8_t kOutdoorLightMask = 0xf0;
  static constexpr uint8_t kIndoorLightMask  = 0x0f;

  static constexpr uint8_t kMaxIndoorLight  = 0x0f;
  static constexpr uint8_t kMaxOutdoorLight  = 0x0f;

  block_id_t id() const { return mType; }
  const BlockDef& type() const;
  bool opaque() const { return mBitFlags & kOpaqueFlag; }
  bool dirty() const  { return mBitFlags & kDirtyFlag;  }

  uint8_t indoorLight() const { return mLight & kIndoorLightMask; }
  uint8_t outdoorLight() const { return (mLight & kOutdoorLightMask) >> 4; }

  void setIndoorLight(uint8_t amount) { mLight = (amount & kIndoorLightMask) | (mLight & (~kIndoorLightMask)); }
  void setOutdoorLight(uint8_t amount) { mLight = ((amount << 4) & kOutdoorLightMask) | (mLight & (~kOutdoorLightMask)); }
  void setSky() { setOutdoorLight(kMaxOutdoorLight); }
  bool exposedToSky() const { return outdoorLight() == kMaxOutdoorLight; }
  void setDirty() { mBitFlags = mBitFlags | kDirtyFlag; }
  void clearDirty() { mBitFlags = mBitFlags & (~kDirtyFlag); }

  static Block kInvalid;
protected:
  void resetFromDef(const BlockDef& def);

  block_id_t mType = 0;
  uint8_t mLight = 0;
  uint8_t mBitFlags = kOpaqueFlag;
};
