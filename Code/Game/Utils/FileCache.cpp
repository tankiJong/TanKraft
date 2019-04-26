#include "FileCache.hpp"
#include "Engine/File/FileSystem.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Game/World/Chunk.hpp"
#include "Engine/File/Utils.hpp"

static FileCache* gInstance = nullptr;

void FileCache::init() {
  FileSystem::Get().mount(kChunkSaveLocationDir, "Saves");

  FileSystem::Get().foreach(kChunkSaveLocationDir, [this](const fs::path& path, const FileSystem&) {
    if(path.extension() == ".chunk") {
      mExistingFiles.insert(path.generic_string());
    }
  });
}

bool FileCache::load(Chunk& chunk) const {
  ChunkCoords coords = chunk.coords();
  std::string path = Stringf(kChunkSaveLocationFormatStr, coords.x, coords.y);

  if(!exists(path)) return false;

  auto physicalPaths = FileSystem::Get().map(path);
  // should only map to one dir
  EXPECTS(physicalPaths.size() == 1);
  
  Blob data = fs::read(physicalPaths[0]);
  chunk.deserialize(data, data.size());

  return true;
}

bool FileCache::save(Chunk& chunk) const {
  ChunkCoords coords = chunk.coords();
  std::string path = Stringf(kChunkSaveLocationFormatStr, coords.x, coords.y);

  auto physicalPaths = FileSystem::Get().map(path);
  // should only map to one dir
  EXPECTS(physicalPaths.size() == 1);

  size_t bufSize = Chunk::kTotalBlockCount * 2 + 16; // worst case
  byte_t* buf = (byte_t*)_alloca(bufSize); 
  size_t total = chunk.serialize(buf, bufSize);

  fs::write(physicalPaths[0], buf, total);

  return true;
}

S<Job::Counter> FileCache::loadAsync(Chunk& chunk) const {
  Job::Decl decl(this, &FileCache::load, chunk);
  return Job::create(decl, Job::CAT_IO );
}

bool FileCache::exists(std::string_view vFile) const {
  return mExistingFiles.find(vFile.data()) != mExistingFiles.end();
}

FileCache& FileCache::get() {
  if(gInstance == nullptr) {
    gInstance = new FileCache();
  }

  return *gInstance;
}
