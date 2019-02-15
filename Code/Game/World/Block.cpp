#include "Block.hpp"
#include "BlockDef.hpp"
void Block::reset(const BlockDef& def) {
  mType = def.id();
  mState = 0u;
}

const BlockDef& Block::type() const {
  return *BlockDef::get(mType);
}
