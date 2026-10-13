#pragma once
#include "services/files-service/abstract-file-indexer.hpp"
#include "services/files-service/abstract-recent-files-provider.hpp"
#include "services/root-item-manager/visit-tracker.hpp"
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

class FileService {

public:
  bool isAvailable() const;

  AbstractFileIndexer *indexer() const;
  AbstractRecentFilesProvider *recentFiles() const;

  void rebuildIndex();

  QFuture<std::vector<IndexerFileResult>> queryAsync(std::string_view query,
                                                     const IndexerQueryParams &params = {});

  QFuture<std::vector<std::filesystem::path>> recentFilesAsync(const RecentFilesParams &params = {});
  // `query` is the search text the file was opened from, used to rank it first for that text again
  void recordAccess(const std::filesystem::path &path, std::string_view query = {});

  void preferencesChanged(const FilePreferences &preferences);

  FileService();

private:
  std::vector<IndexerFileResult> applyVisitHistory(std::vector<IndexerFileResult> results,
                                                   std::string_view query,
                                                   const IndexerQueryParams &params) const;

  VisitTracker m_visits;
  std::unique_ptr<AbstractFileIndexer> m_indexer;
  std::unique_ptr<AbstractRecentFilesProvider> m_recentFiles;
};
