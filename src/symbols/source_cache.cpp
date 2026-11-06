#include <stepwise/symbols/source_cache.hpp>

#include <format>
#include <fstream>

namespace stepwise::symbols {

Result<std::span<const std::string>> SourceCache::lines(const std::string& path) {
    auto cached = files_.find(path);
    if (cached == files_.end()) {
        std::ifstream stream{path};
        if (!stream) {
            return Error{ErrorCode::NotFound, std::format("cannot open source file '{}'", path)};
        }
        std::vector<std::string> lines;
        for (std::string line; std::getline(stream, line);) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
        cached = files_.emplace(path, std::move(lines)).first;
    }
    return std::span<const std::string>{cached->second};
}

}  // namespace stepwise::symbols
