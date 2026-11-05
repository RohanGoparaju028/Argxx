#include "GithubAccount.hpp"

#include <cpr/cpr.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

using json = nlohmann::json;

std::string LoadEnvVar(const std::string& key, const std::string& env_path) {
    if (const auto* existing = std::getenv(key.c_str())) return existing;
    std::ifstream input(env_path);
    std::string line;
    const std::regex assignment(R"(^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$)");
    while (std::getline(input, line)) {
        std::smatch match;
        if (!std::regex_match(line, match, assignment) || match[1].str() != key) continue;
        auto value = match[2].str();
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1, value.size() - 2);
        return value;
    }
    return "";
}

static std::string GetPAT() {
    for (const auto& key : {"GITHUB_PAT", "PAT_TOKEN", "GITHUB_TOKEN", "PAT"}) {
        const auto value = LoadEnvVar(key);
        if (!value.empty()) return value;
    }
    return "";
}

bool AuthenticateGithub() {
    std::string token = GetPAT();
    if (token.empty()) {
        std::cerr << "Error: No PAT found in .env" << std::endl;
        return false;
    }

    auto response = cpr::Get(cpr::Url{"https://api.github.com/user"},
                             cpr::Header{{"Accept", "application/vnd.github+json"},
                                         {"User-Agent", "Argxx"},
                                         {"Authorization", "Bearer " + token}},
                             cpr::ConnectTimeout{5000}, cpr::Timeout{30000});

    return response.status_code == 200;
}

std::string GetRepoFullPath(const std::string& repo_name) {
    std::string token = GetPAT();
    if (token.empty()) {
        std::cerr << "Error: No PAT found in .env" << std::endl;
        return "";
    }

    cpr::Header headers{{"Accept", "application/vnd.github+json"},
                        {"User-Agent", "Argxx"},
                        {"Authorization", "Bearer " + token}};

    std::string full_repo_identifier = repo_name;

    // If repo_name does not contain owner (e.g. "Argxx"), resolve user's login first
    if (repo_name.find('/') == std::string::npos) {
        auto user_res = cpr::Get(cpr::Url{"https://api.github.com/user"}, headers,
                                 cpr::ConnectTimeout{5000}, cpr::Timeout{30000});
        if (user_res.status_code != 200) {
            std::cerr << "Failed to fetch user info: " << user_res.status_code << " - "
                      << user_res.text << std::endl;
            return "";
        }
        auto user_json = json::parse(user_res.text);
        std::string login = user_json["login"].get<std::string>();
        full_repo_identifier = login + "/" + repo_name;
    }

    auto repo_res = cpr::Get(cpr::Url{"https://api.github.com/repos/" + full_repo_identifier},
                             headers, cpr::ConnectTimeout{5000}, cpr::Timeout{30000});
    if (repo_res.status_code != 200) {
        std::cerr << "Failed to find repo: " << repo_res.status_code << " - " << repo_res.text
                  << std::endl;
        return "";
    }

    auto repo_json = json::parse(repo_res.text);
    return repo_json["html_url"].get<std::string>();
}
