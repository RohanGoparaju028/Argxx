#include "Process.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace argxx {
ProcessResult runProcess(const std::vector<std::string>& args, const std::filesystem::path& cwd,
                         double timeout, size_t outputLimit) {
    if (args.empty() || args.front().empty() || timeout <= 0)
        throw std::runtime_error("Empty command or exhausted time budget");
    if (std::any_of(args.begin(), args.end(),
                    [](const auto& arg) { return arg.find('\0') != std::string::npos; }))
        throw std::runtime_error("NUL in argument");
    std::vector<char*> argv;
    std::transform(args.begin(), args.end(), std::back_inserter(argv),
                   [](const auto& arg) { return const_cast<char*>(arg.c_str()); });
    argv.push_back(nullptr);
    int pipeFds[2];
    if (pipe(pipeFds) != 0) throw std::runtime_error("Cannot create process pipe");
    const auto start = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        throw std::runtime_error("Cannot fork process");
    }
    if (pid == 0) {
        close(pipeFds[0]);
        setpgid(0, 0);
        const int nullFd = open("/dev/null", O_RDONLY);
        dup2(nullFd, STDIN_FILENO);
        close(nullFd);
        dup2(pipeFds[1], STDOUT_FILENO);
        dup2(pipeFds[1], STDERR_FILENO);
        close(pipeFds[1]);
        if (chdir(cwd.c_str()) != 0) _exit(126);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipeFds[1]);
    fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
    setpgid(pid, pid);
    ProcessResult result;
    const auto drain = [&]() {
        char buffer[8192];
        ssize_t bytes;
        // Bound each drain so a continuously writing process cannot evade its deadline.
        for (int n = 0; n < 128 && (bytes = read(pipeFds[0], buffer, sizeof(buffer))) > 0; ++n) {
            result.output.append(buffer, static_cast<size_t>(bytes));
            if (result.output.size() > outputLimit) {
                result.truncated = true;
                result.output.erase(0, result.output.size() - outputLimit);
            }
        }
    };
    int status = 0;
    while (true) {
        drain();
        const auto waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) {
            kill(-pid, SIGKILL);
            close(pipeFds[0]);
            throw std::runtime_error("Cannot wait for subprocess");
        }
        result.seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (result.seconds >= timeout) {
            result.timedOut = true;
            kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // Commands may not leave background descendants running after they return.
    kill(-pid, SIGKILL);
    drain();
    close(pipeFds[0]);
    result.seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return result;
}
std::string checkedProcess(const std::vector<std::string>& args, const std::filesystem::path& cwd,
                           double timeout, size_t outputLimit) {
    const auto result = runProcess(args, cwd, timeout, outputLimit);
    if (result.timedOut || result.code != 0 || result.truncated)
        throw std::runtime_error(args.front() + " failed (exit " + std::to_string(result.code) +
                                 (result.timedOut ? ", timed out" : "") +
                                 (result.truncated ? ", output limit exceeded" : "") +
                                 "): " + result.output);
    return result.output;
}
}  // namespace argxx
