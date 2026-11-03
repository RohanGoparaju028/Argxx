#include <chrono>
#include <sstream>

#include "../Agent.cpp"

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        int issuePages = 0;
        auto read = [&](const std::string& path) -> json {
            if (path.find("/commits/") != std::string::npos)
                return {{"sha", "commit123"}, {"commit", {{"tree", {{"sha", "tree123"}}}}}};
            if (path.find("/git/trees/") != std::string::npos)
                return {
                    {"truncated", false},
                    {"tree",
                     json::array(
                         {{{"path", "src"}, {"type", "tree"}, {"sha", "dir"}},
                          {{"path", "src/main.cpp"},
                           {"type", "blob"},
                           {"sha", "code"},
                           {"size", 20}},
                          {{"path", ".env"}, {"type", "blob"}, {"sha", "secret"}, {"size", 20}}})}};
            if (path.find("/git/blobs/") != std::string::npos) {
                require(path.find("secret") == std::string::npos, "Do not fetch .env contents");
                return {{"encoding", "base64"}, {"content", "aW50IG1haW4oKSB7fQ==\n"}};
            }
            if (path.find("/issues?") != std::string::npos) {
                ++issuePages;
                if (issuePages == 1) {
                    auto page = json::array();
                    for (int i = 0; i < 100; ++i)
                        page.push_back({{"pull_request", json::object()}});
                    return page;
                }
                return json::array({{{"number", 7},
                                     {"title", "main bug"},
                                     {"body", nullptr},
                                     {"html_url", "https://example.test/7"}}});
            }
            throw std::runtime_error("Unexpected fixture request: " + path);
        };
        const auto context = inspectRepository("https://github.com/owner/repo.git", read);
        require(context.graph.at("nodes").size() == 4,
                "Graph contains repository root and tree entries");
        require(context.graph.at("edges").at(1).at("from") == "src", "Containment edge parent");
        require(context.issues.size() == 1 && issuePages == 2,
                "Paginate issues and exclude pull requests");
        require(context.sources.at("src/main.cpp") == "int main() {}", "Decode source blob");
        require(repositoryExcerpt(context, "main").find("int main() {}") != std::string::npos,
                "Source retrieval");
        bool rejected = false;
        try {
            inspectRepository("owner/../escape", read);
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected, "Reject repository path traversal");
        rejected = false;
        try {
            inspectRepository("owner/repo", [&](const std::string& path) -> json {
                if (path.find("/git/trees/") != std::string::npos) return {{"truncated", true}};
                return read(path);
            });
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected, "Reject truncated trees");
        const auto config = configureModel();
        require(githubToolEnabled(config.at("tools")),
                "Use existing flattened tools configuration");
        const auto base = initialMessages(config);
        require(base.dump().find("token_env_var") == std::string::npos,
                "Exclude authentication configuration from prompt");

        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto directory =
            std::filesystem::temp_directory_path() / ("argxx-test-" + std::to_string(stamp));
        const std::string issuePath = (directory / ".Argxx/.issue/owner/repo").string() + "/";
        std::istringstream input(
            "\nfirst question\nretry question\nfollow up\n exit \nnever send this\n");
        std::ostringstream output, errors;
        int calls = 0;
        runAgentChat(
            context, base, 1500, issuePath,
            [&](const json& messages, int limit) {
                ++calls;
                require(limit == 1500, "Configured token budget");
                require(messages.back().at("content").get<std::string>().find("int main() {}") !=
                            std::string::npos,
                        "Chat includes repository context");
                if (calls == 1) throw std::runtime_error("Simulated connection error");
                require(messages.size() == (calls == 2 ? 2 : 4),
                        "Retain successful conversation turns only");
                return std::string("Suggested fix");
            },
            input, output, errors);
        require(calls == 3, "Exit and blank lines do not send requests; errors allow retry");
        require(errors.str().find("Simulated connection error") != std::string::npos,
                "Report request failure");
        std::ifstream transcriptFile(issuePath + "chat.json");
        const auto transcript = json::parse(transcriptFile);
        require(transcript.size() == 2, "Save successful turns in .Argxx/.issue");
        saveAgentReport((directory / ".Argxx/.graph/owner/repo/graph.json").string(),
                        context.graph.dump());
        require(std::filesystem::exists(directory / ".Argxx/.graph/owner/repo/graph.json"),
                "Save graph in .Argxx/.graph");
        std::istringstream eofInput;
        runAgentChat(
            context, base, 1500, issuePath,
            [&](const json&, int) {
                throw std::runtime_error("EOF must not call model");
                return std::string();
            },
            eofInput, output, errors);
        std::filesystem::remove_all(directory);
        std::cout << "Agent interaction checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
