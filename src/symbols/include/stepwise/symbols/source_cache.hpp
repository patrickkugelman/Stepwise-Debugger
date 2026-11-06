#pragma once

#include <map>
#include <span>
#include <string>
#include <vector>

#include <stepwise/base/result.hpp>

namespace stepwise::symbols {

class SourceCache {
public:
    Result<std::span<const std::string>> lines(const std::string& path);

private:
    std::map<std::string, std::vector<std::string>> files_;
};

}  // namespace stepwise::symbols
