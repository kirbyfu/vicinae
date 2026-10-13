#pragma once
#include <compare>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <filesystem>

// Records which items were picked and the search text they were picked from, so that typing the same
// text again can rank the previous pick first. Thread-safe: the file service reads it from query
// worker threads.
class VisitTracker {
public:
  struct VisitInfo {
    std::optional<std::uint64_t> lastVisitedAt;
    int visitCount = 0;
    // normalized search text -> unix seconds of the last visit made from it
    std::unordered_map<std::string, std::uint64_t> queries;
  };

  struct Data {
    std::unordered_map<std::string, VisitInfo> visited;
  };

  // How strongly past picks tie an item to the current search text: picked from exactly this text,
  // or from a longer text starting with it. Compares by tier, then by most recent pick.
  struct QueryAffinity {
    enum Tier { None, Prefix, Exact };

    Tier tier = None;
    std::uint64_t lastPickedAt = 0;

    auto operator<=>(const QueryAffinity &) const = default;
  };

  using Affinities = std::unordered_map<std::string, QueryAffinity>;

  VisitTracker(const std::filesystem::path &path);

  static std::string normalizeQuery(std::string_view query);

  void registerVisit(const std::string &key, std::string_view query = {});
  void forget(const std::string &key);
  VisitInfo getVisit(const std::string &key) const;

  // keyed like registerVisit; only items with an affinity to `query` are present
  Affinities affinities(std::string_view query) const;

private:
  static constexpr std::size_t MAX_QUERIES_PER_ITEM = 16;

  void loadFromDisk();
  void saveToDisk();

  std::filesystem::path m_path;
  std::string m_buf;
  Data m_data;
  mutable std::mutex m_mutex;
};
