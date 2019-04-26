#pragma once
#include "Engine/Core/common.hpp"
#include <unordered_set>
#include "Engine/File/Path.hpp"
#include "Engine/Async/Job.hpp"

class Chunk;

class FileCache {
public:
  static constexpr const char* kChunkSaveLocationFormatStr = "/Saves/Chunk_%i,%i.chunk";
  static constexpr const char* kChunkSaveLocationDir = "/Saves";
  static FileCache& get();
  void init();
  bool load(Chunk& chunk) const;
  bool save(Chunk& chunk) const;

  S<Job::Counter> loadAsync(Chunk& chunk) const;
  bool exists(std::string_view vFile) const;
protected:
  FileCache() = default;
  bool mReady = false;
  std::unordered_set<std::string, std::hash<std::string_view>> mExistingFiles;
};
