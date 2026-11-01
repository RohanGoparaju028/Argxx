#include "GithubAccount.hpp"

#include <cpr/cpr.h>
#include <dotenv.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

std::string LoadEnvVar(const std::string& key, const std::string& env_path) {
    dotenv::init(dotenv::Preserve, env_path.c_str());
    return dotenv::getenv(key.c_str());
}

static std::string GetPAT() {
    dotenv::init(dotenv::Preserve);
    std::string token = dotenv::getenv("PAT_TOKEN");
    if (token.empty()) token = dotenv::getenv("GITHUB_TOKEN");
    if (token.empty()) token = dotenv::getenv("PAT");
    return token;
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
                                         {"Authorization", "Bearer " + token}});

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
        auto user_res = cpr::Get(cpr::Url{"https://api.github.com/user"}, headers);
        if (user_res.status_code != 200) {
            std::cerr << "Failed to fetch user info: " << user_res.status_code << " - "
                      << user_res.text << std::endl;
            return "";
        }
        auto user_json = json::parse(user_res.text);
        std::string login = user_json["login"].get<std::string>();
        full_repo_identifier = login + "/" + repo_name;
    }

    auto repo_res =
        cpr::Get(cpr::Url{"https://api.github.com/repos/" + full_repo_identifier}, headers);
    if (repo_res.status_code != 200) {
        std::cerr << "Failed to find repo: " << repo_res.status_code << " - " << repo_res.text
                  << std::endl;
        return "";
    }

    auto repo_json = json::parse(repo_res.text);
    return repo_json["html_url"].get<std::string>();
}
