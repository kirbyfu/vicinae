#include "services/root-item-manager/visit-tracker.hpp"
#include <glaze/core/reflect.hpp>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>
#include <glaze/util/key_transformers.hpp>
#include <qlogging.h>
#include <QDateTime>
#include <QString>
#include <algorithm>

namespace fs = std::filesystem;

VisitTracker::VisitTracker(const fs::path &path) : m_path(path) { loadFromDisk(); }

std::string VisitTracker::normalizeQuery(std::string_view query) {
  return QString::fromUtf8(query.data(), static_cast<qsizetype>(query.size()))
      .simplified()
      .toLower()
      .toStdString();
}

void VisitTracker::registerVisit(const std::string &key, std::string_view query) {
  std::scoped_lock lock(m_mutex);
  auto const now = static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());
  VisitInfo &data = m_data.visited[key];

  data.lastVisitedAt = now;
  ++data.visitCount;

  if (auto normalized = normalizeQuery(query); !normalized.empty()) {
    data.queries[std::move(normalized)] = now;

    while (data.queries.size() > MAX_QUERIES_PER_ITEM) {
      data.queries.erase(std::ranges::min_element(data.queries, {}, [](auto &&pair) { return pair.second; }));
    }
  }

  saveToDisk();
}

VisitTracker::VisitInfo VisitTracker::getVisit(const std::string &key) const {
  std::scoped_lock lock(m_mutex);
  if (auto it = m_data.visited.find(key); it != m_data.visited.end()) { return it->second; }
  return {};
}

void VisitTracker::forget(const std::string &key) {
  std::scoped_lock lock(m_mutex);
  m_data.visited.erase(key);
  saveToDisk();
}

VisitTracker::Affinities VisitTracker::affinities(std::string_view query) const {
  Affinities result;
  auto const normalized = normalizeQuery(query);

  if (normalized.empty()) { return result; }

  std::scoped_lock lock(m_mutex);

  for (const auto &[key, info] : m_data.visited) {
    QueryAffinity best;

    for (const auto &[picked, at] : info.queries) {
      auto const tier = picked == normalized              ? QueryAffinity::Exact
                        : picked.starts_with(normalized) ? QueryAffinity::Prefix
                                                         : QueryAffinity::None;

      if (tier != QueryAffinity::None) { best = std::max(best, QueryAffinity{tier, at}); }
    }

    if (best.tier != QueryAffinity::None) { result.emplace(key, best); }
  }

  return result;
}

void VisitTracker::loadFromDisk() {
  std::error_code ec;

  if (!fs::exists(m_path, ec)) { return; }

  if (auto error = glz::read_file_jsonc(m_data, m_path.string(), m_buf)) {
    qWarning() << "Failed to load visits file from" << m_path.string().c_str() << glz::format_error(error);
  }
}

void VisitTracker::saveToDisk() {
  if (auto error = glz::write_file_json(m_data, m_path.string(), m_buf)) {
    qWarning() << "Failed to save visit to" << m_path.string().c_str() << glz::format_error(error);
  }
}

template <> struct glz::meta<VisitTracker::VisitInfo> : glz::snake_case {};
