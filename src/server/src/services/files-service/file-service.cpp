#include "file-service.hpp"
#if defined(Q_OS_LINUX)
#include "file-indexer/file-indexer.hpp"
#include "linux/xbel-recent-files-provider.hpp"
#elif defined(Q_OS_MACOS)
#include "macos/spotlight-file-indexer.hpp"
#include "macos/spotlight-recent-files-provider.hpp"
#elif defined(Q_OS_WIN)
#include "windows/win-file-indexer-selector.hpp"
#include "windows/win-recent-files-provider.hpp"
#else
#include "dummy-file-indexer.hpp"
#include "dummy-recent-files-provider.hpp"
#endif

#include <QString>
#include <QtCore/qfuture.h>
#include <algorithm>
#include <unordered_set>
#include "vicinae.hpp"

namespace fs = std::filesystem;

namespace {

std::string visitKey(const fs::path &path) { return QString::fromStdWString(path.wstring()).toStdString(); }

// visit keys keep the path as opened (to rebuild it for display); lookups ignore case where the
// file system does
std::string lookupKey(std::string_view key) {
#if defined(Q_OS_WIN)
  return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())).toLower().toStdString();
#else
  return std::string{key};
#endif
}

} // namespace

AbstractFileIndexer *FileService::indexer() const { return m_indexer.get(); }

AbstractRecentFilesProvider *FileService::recentFiles() const { return m_recentFiles.get(); }

QFuture<std::vector<IndexerFileResult>> FileService::queryAsync(std::string_view query,
                                                                const IndexerQueryParams &params) {
  // async launch: re-adding previously opened files stats them, which must not run on the UI thread
  return m_indexer->queryAsync(query, params)
      .then(QtFuture::Launch::Async,
            [this, q = std::string(query), params](std::vector<IndexerFileResult> results) {
              return applyVisitHistory(std::move(results), q, params);
            });
}

std::vector<IndexerFileResult> FileService::applyVisitHistory(std::vector<IndexerFileResult> results,
                                                              std::string_view query,
                                                              const IndexerQueryParams &params) const {
  using Affinity = VisitTracker::QueryAffinity;

  std::unordered_map<std::string, std::pair<std::string, Affinity>> picked;

  for (auto &[key, affinity] : m_visits.affinities(query)) {
    picked.emplace(lookupKey(key), std::pair{key, affinity});
  }

  if (picked.empty()) { return results; }

  struct Ranked {
    IndexerFileResult result;
    Affinity affinity;
  };

  std::vector<Ranked> ranked;
  std::unordered_set<std::string> seen;

  ranked.reserve(results.size() + picked.size());

  for (IndexerFileResult &result : results) {
    auto key = lookupKey(visitKey(result.path));
    Affinity affinity;

    if (auto it = picked.find(key); it != picked.end()) { affinity = it->second.second; }

    seen.insert(std::move(key));
    ranked.push_back({std::move(result), affinity});
  }

  // files opened from this text before that the indexer did not return (e.g. outside its window)
  for (const auto &[key, entry] : picked) {
    if (seen.contains(key)) { continue; }

    fs::path const path = QString::fromStdString(entry.first).toStdWString();
    std::error_code ec;
    auto const status = fs::status(path, ec);

    if (ec || !fs::exists(status)) { continue; }

    auto const category = vicinae::fileCategoryFor(path, fs::is_directory(status));

    if (params.category && *params.category != category) { continue; }

    ranked.push_back({IndexerFileResult{.path = path, .category = category}, entry.second});
  }

  std::ranges::stable_sort(ranked, std::ranges::greater{}, &Ranked::affinity);

  size_t const end = std::min(static_cast<size_t>(std::max(params.limit, 0)), ranked.size());

  results.clear();
  results.reserve(end);

  for (size_t i = 0; i < end; ++i) {
    ranked[i].result.rank = static_cast<double>(end - i);
    results.emplace_back(std::move(ranked[i].result));
  }

  return results;
}

void FileService::rebuildIndex() { m_indexer->rebuildIndex(); }

QFuture<std::vector<fs::path>> FileService::recentFilesAsync(const RecentFilesParams &params) {
  return m_recentFiles->listAsync(params);
}

void FileService::recordAccess(const fs::path &path, std::string_view query) {
  m_recentFiles->recordAccess(path);
  m_visits.registerVisit(visitKey(path), query);
}

void FileService::preferencesChanged(const FilePreferences &preferences) {
  m_indexer->preferencesChanged(preferences);
}

bool FileService::isAvailable() const { return m_indexer->isAvailable(); }

FileService::FileService() : m_visits(Omnicast::dataDir() / "file-visits.json") {
#if defined(Q_OS_LINUX)
  m_indexer = std::make_unique<FileIndexer>();
  m_recentFiles = std::make_unique<XbelRecentFilesProvider>();
#elif defined(Q_OS_MACOS)
  m_indexer = std::make_unique<SpotlightFileIndexer>();
  m_recentFiles = std::make_unique<SpotlightRecentFilesProvider>();
#elif defined(Q_OS_WIN)
  m_indexer = std::make_unique<WinFileIndexerSelector>();
  m_recentFiles = std::make_unique<WinRecentFilesProvider>();
#else
  m_indexer = std::make_unique<DummyFileIndexer>();
  m_recentFiles = std::make_unique<DummyRecentFilesProvider>();
#endif
}
