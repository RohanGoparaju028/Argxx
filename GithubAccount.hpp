#ifndef GITHUB_ACCOUNT_HPP
#define GITHUB_ACCOUNT_HPP

#include <string>

std::string LoadEnvVar(const std::string& key, const std::string& env_path = ".env");
bool AuthenticateGithub();
std::string GetRepoFullPath(const std::string& repo_name);

#endif  // GITHUB_ACCOUNT_HPP
