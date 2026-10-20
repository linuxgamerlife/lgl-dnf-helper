#include "core/commandrunner.h"

#include <giomm/subprocess.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>

#include <memory>

namespace dnfhelper {

namespace {

std::string validText(const Glib::RefPtr<Glib::Bytes> &bytes)
{
    if (!bytes)
        return {};
    gsize size = 0;
    const auto *data = static_cast<const char *>(bytes->get_data(size));
    if (!data || size == 0)
        return {};
    gchar *valid = g_utf8_make_valid(data, static_cast<gssize>(size));
    std::string text(valid);
    g_free(valid);
    return text;
}

// Shared between the timeout and the completion, whichever comes first.
struct Pending {
    Glib::RefPtr<Gio::Subprocess> process;
    CommandCallback done;
    sigc::connection timer;
    bool timedOut{false};
};

} // namespace

SubprocessRunner::SubprocessRunner(const unsigned int timeoutMs)
    : m_timeoutMs(timeoutMs)
{
}

void SubprocessRunner::run(const std::string &program, const std::vector<std::string> &arguments,
                           CommandCallback done)
{
    std::vector<std::string> argv{program};
    argv.insert(argv.end(), arguments.begin(), arguments.end());

    auto pending = std::make_shared<Pending>();
    pending->done = std::move(done);
    try {
        pending->process = Gio::Subprocess::create(
            argv, Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_PIPE);
    } catch (const Glib::Error &error) {
        CommandResult result;
        result.failure = "Failed to start command: " + std::string(error.what());
        // Report on the main loop, as for a started command.
        Glib::signal_idle().connect_once([pending, result] { pending->done(result); });
        return;
    }

    pending->timer = Glib::signal_timeout().connect(
        [pending] {
            pending->timedOut = true;
            pending->process->force_exit();
            return false;
        },
        m_timeoutMs);

    pending->process->communicate_async(
        {}, [pending](const Glib::RefPtr<Gio::AsyncResult> &asyncResult) {
            pending->timer.disconnect();
            CommandResult result;
            try {
                const auto [out, err] = pending->process->communicate_finish(asyncResult);
                result.out = validText(out);
                result.err = validText(err);
            } catch (const Glib::Error &error) {
                result.failure = "Command read error: " + std::string(error.what());
            }
            if (pending->timedOut)
                result.failure = "Command timed out";
            else if (result.failure.empty() && pending->process->get_if_signaled())
                result.failure = "Command crashed";
            else if (result.failure.empty())
                result.exitCode = pending->process->get_exit_status();
            pending->done(result);
        });
}

std::string commandLineFor(const std::string &program, const std::vector<std::string> &arguments)
{
    std::string line = program;
    for (const std::string &argument : arguments) {
        std::string escaped;
        for (const char c : argument) {
            if (c == '\'')
                escaped += "'\\''";
            else
                escaped += c;
        }
        line += " '" + escaped + "'";
    }
    return line;
}

bool programAvailable(const std::string &program)
{
    return !Glib::find_program_in_path(program).empty();
}

} // namespace dnfhelper
