#include "win-file-candidates.hpp"
#include <qstring.h>
#include <algorithm>
#include "fuzzy/fuzzy-searchable.hpp"

vicinae::FileCategory winFileCategory(const WinFileCandidate &candidate) {
  return candidate.isDirectory ? vicinae::FileCategory::Directory
                               : vicinae::fileCategoryFor(candidate.path, false);
}

std::vector<IndexerFileResult> orderedWinFileResults(std::vector<WinFileCandidate> candidates,
                                                     const IndexerQueryParams &params) {
  std::vector<IndexerFileResult> results;

  results.reserve(candidates.size());

  for (WinFileCandidate &candidate : candidates) {
    auto const category = winFileCategory(candidate);

    if (params.category && *params.category != category) { continue; }

    results.emplace_back(IndexerFileResult{.path = std::move(candidate.path),
                                           .rank = static_cast<double>(candidates.size() - results.size()),
                                           .category = category,
                                           .mimeType = std::move(candidate.mimeType)});
  }

  return results;
}

std::vector<IndexerFileResult> rankedWinFileResults(std::vector<WinFileCandidate> candidates,
                                                    std::string_view query, const IndexerQueryParams &params) {
  struct Scored {
    IndexerFileResult result;
    int score = 0;
  };

  fuzzy::Query const fuzzyQuery{query};
  std::vector<Scored> scored;

  scored.reserve(candidates.size());

  for (WinFileCandidate &candidate : candidates) {
    auto const category = winFileCategory(candidate);

    if (params.category && *params.category != category) { continue; }

    // via QString: path::string() throws for names outside the ANSI code page
    std::string const name = QString::fromStdWString(candidate.path.filename().wstring()).toStdString();
    auto const m = fuzzy::scoreWeighted({{name, 1.0}}, fuzzyQuery);

    scored.emplace_back(Scored{.result = IndexerFileResult{.path = std::move(candidate.path),
                                                           .category = category,
                                                           .mimeType = std::move(candidate.mimeType)},
                               .score = m.accepted() ? m.score : 0});
  }

  std::ranges::stable_sort(scored, std::ranges::greater{}, &Scored::score);

  std::vector<IndexerFileResult> results;
  size_t const end = std::min(static_cast<size_t>(std::max(params.limit, 0)), scored.size());

  results.reserve(end);

  for (size_t i = 0; i < end; ++i) {
    scored[i].result.rank = static_cast<double>(end - i);
    results.emplace_back(std::move(scored[i].result));
  }

  return results;
}
