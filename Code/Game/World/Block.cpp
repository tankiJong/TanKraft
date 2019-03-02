#include "Block.hpp"
#include "BlockDef.hpp"

Block Block::kInvalid;

void Block::resetFromDef(const BlockDef& def) {
  mType = def.id();
  mBitFlags = 0;
  mBitFlags |= def.opaque() ? kOpaqueFlag : 0x0;
  // mLight |= def.emissive() & kIndoorLightMask;
}

const BlockDef& Block::type() const {
  return *BlockDef::get(mType);
}
