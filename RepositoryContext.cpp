#include "RepositoryContext.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <regex>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using json = nlohmann::json;

bool readableSource(const json& entry) {
    if (entry.at("type") != "blob" || entry.value("mode", "") == "120000" ||
        entry.value("size", 0LL) > 24000)
        return false;
    const std::filesystem::path path(entry.at("path").get<std::string>());
    if (std::any_of(path.begin(), path.end(), [](const auto& part) {
            const auto name = part.string();
            return name.empty() || name.front() == '.' || name == "build" || name == "vendor" ||
                   name == "node_modules";
        }))
        return false;
    static const std::set<std::string> extensions = {".cpp", ".hpp", ".h",    ".c",   ".cc",
                                                     ".py",  ".js",  ".ts",   ".tsx", ".jsx",
                                                     ".rs",  ".go",  ".java", ".md"};
    return extensions.count(path.extension().string()) || path.filename() == "CMakeLists.txt";
}

std::string decodeBlob(const std::string& encoded) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int value = 0;
    int bits = 0;
    std::string decoded;
    for (unsigned char c : encoded) {
        if (std::isspace(c)) continue;
        if (c == '=') break;
        const auto index = alphabet.find(c);
        if (index == std::string::npos) throw std::runtime_error("Invalid GitHub base64 content.");
        value = (value << 6) | static_cast<unsigned int>(index);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded += static_cast<char>((value >> bits) & 255);
        }
    }
    return decoded;
}

std::set<std::string> words(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    static const std::regex word("[a-z0-9_]{3,}");
    std::set<std::string> result;
    for (std::sregex_iterator it(text.begin(), text.end(), word), end; it != end; ++it)
        result.insert(it->str());
    return result;
}
}  // namespace

RepositoryContext inspectRepository(const std::string& repo, const RepositoryReader& read) {
    std::string name = repo;
    const std::string prefix = "https://github.com/";
    if (name.compare(0, prefix.size(), prefix) == 0) name.erase(0, prefix.size());
    if (!name.empty() && name.back() == '/') name.pop_back();
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git") name.resize(name.size() - 4);
    if (!std::regex_match(name, std::regex("[A-Za-z0-9_-]+/[A-Za-z0-9_.-]+")) ||
        name.substr(name.find('/') + 1) == "." || name.substr(name.find('/') + 1) == "..")
        throw std::runtime_error("Expected owner/repository or a GitHub repository URL.");
    const std::string api = "/repos/" + name;
    // HEAD resolves the default branch without interpolating a branch name into a URL.
    const auto commit = read(api + "/commits/HEAD");
    const auto tree =
        read(api + "/git/trees/" + commit.at("commit").at("tree").at("sha").get<std::string>() +
             "?recursive=1");
    if (tree.value("truncated", false))
        throw std::runtime_error(
            "GitHub truncated the file tree; refusing to save an incomplete graph.");
    RepositoryContext context{json::object(), json::array(), json::object()};
    context.graph = {{"repository", name},
                     {"commit", commit.at("sha")},
                     {"nodes", json::array({{{"path", ""}, {"type", "root"}}})},
                     {"edges", json::array()},
                     {"sampled_files", json::array()},
                     {"scope",
                      "File containment graph. Source analysis samples up to 40 files / 120 KB; "
                      "not an AST or call graph."}};
    size_t bytes = 0;
    for (const auto& entry : tree.at("tree")) {
        const auto path = entry.at("path").get<std::string>();
        context.graph["nodes"].push_back({{"path", path}, {"type", entry.at("type")}});
        context.graph["edges"].push_back(
            {{"from", std::filesystem::path(path).parent_path().generic_string()},
             {"to", path},
             {"relation", "contains"}});
        if (!readableSource(entry) || context.sources.size() >= 40 ||
            bytes + entry.value("size", 0ULL) > 120000)
            continue;
        const auto blob = read(api + "/git/blobs/" + entry.at("sha").get<std::string>());
        if (blob.at("encoding") != "base64")
            throw std::runtime_error("Unsupported GitHub blob encoding.");
        const auto source = decodeBlob(blob.at("content").get<std::string>());
        if (source.find('\0') != std::string::npos) continue;
        // Replace invalid UTF-8 before sending source excerpts as JSON to Ollama.
        context.sources[path] =
            json::parse(json(source).dump(-1, ' ', false, json::error_handler_t::replace));
        bytes += source.size();
        context.graph["sampled_files"].push_back(path);
    }
    for (int page = 1;; ++page) {
        const auto batch =
            read(api + "/issues?state=open&per_page=100&page=" + std::to_string(page));
        if (!batch.is_array()) throw std::runtime_error("Invalid GitHub issues response.");
        for (const auto& issue : batch) {
            if (!issue.contains("pull_request")) context.issues.push_back(issue);
        }
        if (batch.size() < 100) break;
    }
    return context;
}

std::string repositoryExcerpt(const RepositoryContext& context, const std::string& query) {
    const auto terms = words(query);
    std::vector<std::pair<int, std::string>> ranked;
    for (auto it = context.sources.begin(); it != context.sources.end(); ++it) {
        const auto tokens = words(it.key() + " " + it.value().get<std::string>());
        const int score = std::accumulate(
            terms.begin(), terms.end(), 0,
            [&](int sum, const auto& term) { return sum + static_cast<int>(tokens.count(term)); });
        ranked.emplace_back(-score, it.key());
    }
    std::sort(ranked.begin(), ranked.end());
    std::string result = "Repository: " + context.graph.at("repository").get<std::string>() +
                         "\nCommit: " + context.graph.at("commit").get<std::string>() +
                         "\nPartial source excerpts, ranked by keyword overlap:\n";
    for (const auto& item : ranked) {
        const std::string excerpt =
            "\nFILE: " + item.second + "\n" +
            context.sources.at(item.second).get<std::string>().substr(0, 5000) +
            "\n[excerpt ends]\n";
        if (result.size() + excerpt.size() > 24000) break;
        result += excerpt;
    }
    return result;
}

void saveAgentReport(const std::string& path, const std::string& text) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream output(path);
    output << text << '\n';
    output.close();
    if (!output) throw std::runtime_error("Could not write report: " + path);
}
