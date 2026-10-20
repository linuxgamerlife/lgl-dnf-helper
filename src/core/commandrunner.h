#pragma once

#include <functional>
#include <string>
#include <vector>

namespace dnfhelper {

struct CommandResult {
    // Set when the program could not be started, crashed or timed out; exitCode is then -1.
    std::string failure;
    int exitCode{-1};
    std::string out;
    std::string err;

    bool ok() const { return failure.empty() && exitCode == 0; }
};

using CommandCallback = std::function<void(const CommandResult &)>;

// Runs an external program without a shell and reports its result on the main loop.
class CommandRunner {
public:
    virtual ~CommandRunner() = default;
    virtual void run(const std::string &program, const std::vector<std::string> &arguments,
                     CommandCallback done) = 0;
};

// Gio::Subprocess with stdout and stderr captured, killed after the timeout. Output that is not
// valid UTF-8 is repaired so it can be shown.
class SubprocessRunner final : public CommandRunner {
public:
    explicit SubprocessRunner(unsigned int timeoutMs = 30000);
    void run(const std::string &program, const std::vector<std::string> &arguments,
             CommandCallback done) override;

private:
    unsigned int m_timeoutMs;
};

// The program and its arguments as a copyable shell line, for the log.
std::string commandLineFor(const std::string &program, const std::vector<std::string> &arguments);

// True when the program is found on PATH.
bool programAvailable(const std::string &program);

} // namespace dnfhelper
