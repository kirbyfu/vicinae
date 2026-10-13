#include <QtConcurrent/QtConcurrentRun>
#include <qlogging.h>
#include <qstring.h>
#include <algorithm>
#include <chrono>
#include <optional>
#include <span>
#include <string_view>
#include <common/file-category.hpp>
#include "everything-file-indexer.hpp"
#include "everything-pipe-client.hpp"
#include "everything-window-client.hpp"

namespace {

constexpr char REGEX_PREFIX = '@';
constexpr std::chrono::milliseconds SLOW_QUERY_THRESHOLD{1000};
constexpr int MIN_CANDIDATE_POOL = 250;
constexpr int CANDIDATE_POOL_MULTIPLIER = 5;

std::optional<std::wstring> categoryFilter(vicinae::FileCategory category) {
  using vicinae::FileCategory;

  if (category == FileCategory::Directory) { return L"folder:"; }

  bool const other = category == FileCategory::Other;
  std::span<const FileCategory> const categories =
      other ? std::span<const FileCategory>{vicinae::EXTENSION_CATEGORIES}
            : std::span<const FileCategory>{&category, 1};
  std::wstring filter = other ? L"file: !ext:" : L"ext:";
  bool first = true;

  for (FileCategory c : categories) {
    for (std::string_view ext : vicinae::extensionsForCategory(c)) {
      if (!first) { filter += L';'; }
      first = false;
      filter += std::wstring{ext.begin(), ext.end()};
    }
  }

  if (first) { return std::nullopt; }

  return filter;
}

EverythingSearch buildSearch(std::string_view query, const IndexerQueryParams &params) {
  EverythingSearch search{.maxResults = params.limit};

  if (query.starts_with(REGEX_PREFIX)) {
    search.regex = true;
    query.remove_prefix(1);
  }

  search.text =
      QString::fromUtf8(query.data(), static_cast<qsizetype>(query.size())).trimmed().toStdWString();

  if (search.text.empty()) { search.regex = false; }

  if (params.category && !search.regex) {
    if (auto filter = categoryFilter(*params.category)) {
      search.text = search.text.empty() ? *filter : *filter + L' ' + search.text;
    }
  }

  return search;
}

} // namespace

std::unique_ptr<EverythingClient> connectEverythingClient(const std::wstring &instanceName) {
  if (auto pipe = EverythingPipeClient::connect(instanceName)) { return pipe; }
  return EverythingWindowClient::connect();
}

bool EverythingFileIndexer::ensureClient() const {
  if (m_client && m_client->isConnected()) { return true; }

  m_client = connectEverythingClient(m_instanceName);

  return m_client != nullptr;
}

bool EverythingFileIndexer::isAvailable() const {
  // Called from the UI thread (backend selection, extension launch). Never wait behind an
  // in-flight query: it can be blocked on Everything's IPC for as long as Everything is stalled.
  std::unique_lock lock(m_mutex, std::try_to_lock);

  if (!lock.owns_lock()) { return m_available; }

  m_available = ensureClient();
  return m_available;
}

void EverythingFileIndexer::setInstanceName(std::wstring name) {
  std::scoped_lock lock(m_mutex);

  if (name == m_instanceName) { return; }

  m_instanceName = std::move(name);
  m_client.reset();
}

std::vector<IndexerFileResult> EverythingFileIndexer::runQuery(const std::string &query,
                                                               const IndexerQueryParams &params) {
  if (params.limit <= 0) { return {}; }

  EverythingSearch search = buildSearch(query, params);

  if (search.text.empty()) { return {}; }

  // Everything orders by date modified; fetch a wider window so the best name matches in it can be
  // ranked first. Regex and category-only listings keep Everything's order.
  std::string const nameQuery = QString::fromStdString(query).trimmed().toStdString();
  bool const rankByName = !search.regex && !nameQuery.empty();

  if (rankByName) { search.maxResults = std::max(MIN_CANDIDATE_POOL, params.limit * CANDIDATE_POOL_MULTIPLIER); }

  std::vector<WinFileCandidate> candidates;

  {
    std::scoped_lock lock(m_mutex);

    m_available = ensureClient();

    if (!m_available) { return {}; }

    auto const started = std::chrono::steady_clock::now();
    auto found = m_client->search(search);
    auto const elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

    if (elapsed >= SLOW_QUERY_THRESHOLD) {
      qWarning() << "EverythingFileIndexer: query" << QString::fromStdWString(search.text) << "took"
                 << elapsed.count() << "ms";
    }

    if (!found) {
      m_client.reset();
      m_available = false;
      return {};
    }

    candidates = std::move(*found);
  }

  if (rankByName) { return rankedWinFileResults(std::move(candidates), nameQuery, params); }

  return orderedWinFileResults(std::move(candidates), params);
}

QFuture<std::vector<IndexerFileResult>> EverythingFileIndexer::queryAsync(std::string_view query,
                                                                          const IndexerQueryParams &params) {
  return QtConcurrent::run([this, params, q = std::string(query)]() { return runQuery(q, params); });
}
