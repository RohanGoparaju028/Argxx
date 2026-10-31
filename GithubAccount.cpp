#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "GithubAccount.hpp"

using json = nlohmann::json;

std::string LoadEnvVar(const std::string& key, const std::string& env_path) {
    std::ifstream file(env_path);
    if (!file.is_open()) {
        return "";
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip leading whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));

        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string k = line.substr(0, pos);
            std::string v = line.substr(pos + 1);

            auto trim = [](std::string& s) {
                while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
                while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
            };
            trim(k);
            trim(v);

            // Strip enclosing quotes
            if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) {
                v = v.substr(1, v.size() - 2);
            }

            if (k == key) {
                return v;
            }
        }
    }
    return "";
}

static std::string GetPAT() {
    std::string token = LoadEnvVar("PAT_TOKEN");
    if (token.empty()) token = LoadEnvVar("GITHUB_TOKEN");
    if (token.empty()) token = LoadEnvVar("PAT");
    if (token.empty()) {
        const char* env_p = std::getenv("PAT_TOKEN");
        if (!env_p) env_p = std::getenv("GITHUB_TOKEN");
        if (env_p) token = env_p;
    }
    return token;
}

bool AuthenticateGithub() {
    std::string token = GetPAT();
    if (token.empty()) {
        std::cerr << "Error: No PAT found in .env" << std::endl;
        return false;
    }

    auto response = cpr::Get(
        cpr::Url{"https://api.github.com/user"},
        cpr::Header{
            {"Accept", "application/vnd.github+json"},
            {"User-Agent", "Argxx"},
            {"Authorization", "Bearer " + token}
        }
    );

    return response.status_code == 200;
}

std::string GetRepoFullPath(const std::string& repo_name) {
    std::string token = GetPAT();
    if (token.empty()) {
        std::cerr << "Error: No PAT found in .env" << std::endl;
        return "";
    }

    cpr::Header headers{
        {"Accept", "application/vnd.github+json"},
        {"User-Agent", "Argxx"},
        {"Authorization", "Bearer " + token}
    };

    std::string full_repo_identifier = repo_name;

    // If repo_name does not contain owner (e.g. "Argxx"), resolve user's login first
    if (repo_name.find('/') == std::string::npos) {
        auto user_res = cpr::Get(cpr::Url{"https://api.github.com/user"}, headers);
        if (user_res.status_code != 200) {
            std::cerr << "Failed to fetch user info: " << user_res.status_code << " - " << user_res.text << std::endl;
            return "";
        }
        auto user_json = json::parse(user_res.text);
        std::string login = user_json["login"].get<std::string>();
        full_repo_identifier = login + "/" + repo_name;
    }

    auto repo_res = cpr::Get(cpr::Url{"https://api.github.com/repos/" + full_repo_identifier}, headers);
    if (repo_res.status_code != 200) {
        std::cerr << "Failed to find repo: " << repo_res.status_code << " - " << repo_res.text << std::endl;
        return "";
    }

    auto repo_json = json::parse(repo_res.text);
    return repo_json["html_url"].get<std::string>();
}
