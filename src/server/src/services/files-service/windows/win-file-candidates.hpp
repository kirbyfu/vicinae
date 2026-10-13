#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <common/file-category.hpp>
#include "services/files-service/abstract-file-indexer.hpp"

struct WinFileCandidate {
  std::filesystem::path path;
  std::optional<std::string> mimeType;
  bool isDirectory = false;
};

vicinae::FileCategory winFileCategory(const WinFileCandidate &candidate);
std::vector<IndexerFileResult> orderedWinFileResults(std::vector<WinFileCandidate> candidates,
                                                     const IndexerQueryParams &params);

// Ranks candidates by how well their file name matches `query`, keeping at most `params.limit`.
// Candidates the matcher rejects (e.g. matched on their folder or through search syntax) are kept
// after the accepted ones; ties keep the input order.
std::vector<IndexerFileResult> rankedWinFileResults(std::vector<WinFileCandidate> candidates,
                                                    std::string_view query, const IndexerQueryParams &params);
