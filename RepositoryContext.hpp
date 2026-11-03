#ifndef REPOSITORY_CONTEXT_HPP
#define REPOSITORY_CONTEXT_HPP

#include <functional>
#include <nlohmann/json.hpp>
#include <string>

struct RepositoryContext {
    nlohmann::json graph;
    nlohmann::json issues;
    nlohmann::json sources;
};

// Relative GitHub API requests also allow offline fixture tests.
using RepositoryReader = std::function<nlohmann::json(const std::string&)>;
RepositoryContext inspectRepository(const std::string& repo, const RepositoryReader& read);
std::string repositoryExcerpt(const RepositoryContext& context, const std::string& query);
void saveAgentReport(const std::string& path, const std::string& text);

#endif
