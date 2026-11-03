#include <iostream>

#include "Agent.hpp"
#include "Folder.hpp"
#include "GithubAccount.hpp"

using namespace std;

int main() {
    string repo_name = "Argxx";  // for example
    string full_path = GetRepoFullPath(repo_name);

    if (!full_path.empty()) {
        cout << "Full Repo Path: " << full_path << endl;
        createGraphFolder();
        createIssueFolder();
        createAgent(full_path);
    } else {
        cout << "Could not retrieve repo path." << endl;
    }

    return 0;
}
