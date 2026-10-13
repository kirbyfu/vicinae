#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include "everything-client.hpp"
#include "services/files-service/abstract-file-indexer.hpp"

class EverythingFileIndexer : public AbstractFileIndexer {
public:
  void start() override {}
  void rebuildIndex() override {}
  void preferencesChanged(const FilePreferences &) override {}
  bool isAvailable() const override;
  std::chrono::milliseconds queryDebounce() const override { return std::chrono::milliseconds{0}; }

  QFuture<std::vector<IndexerFileResult>> queryAsync(std::string_view query,
                                                     const IndexerQueryParams &params = {}) override;

  void setInstanceName(std::wstring name);

private:
  bool ensureClient() const;
  std::vector<IndexerFileResult> runQuery(const std::string &query, const IndexerQueryParams &params);

  mutable std::mutex m_mutex;
  mutable std::unique_ptr<EverythingClient> m_client;
  // last known availability, served to UI-thread callers while a query holds m_mutex
  mutable std::atomic<bool> m_available{false};
  std::wstring m_instanceName;
};
