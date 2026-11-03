#include <iostream>

#include "Agent.hpp"
#include "Folder.hpp"
#include "GithubAccount.hpp"

int main() {
    std::string repo_name;
    std::cout << "Enter your repo name: ";
    std::cin >> repo_name;
    std::string full_path = GetRepoFullPath(repo_name);

    if (!full_path.empty()) {
        std::cout << "Full Repo Path: " << full_path << std::endl;
        createGraphFolder();
        createIssueFolder();
        createAgent(full_path);
    } else {
        std::cout << "Could not retrieve repo path." << std::endl;
    }

    return 0;
}
