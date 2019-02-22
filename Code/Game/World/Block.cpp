#include "Block.hpp"
#include "BlockDef.hpp"

Block Block::invalid;

void Block::reset(const BlockDef& def) {
  mType = def.id();
  mBitFlags = 0;
  mBitFlags |= def.opaque() ? kOpaqueFlag : 0x0;
}

const BlockDef& Block::type() const {
  return *BlockDef::get(mType);
}
