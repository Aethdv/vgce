#include "process/process.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace vgce::process {

class Process::ProcessImpl {
public:
    ProcessImpl(const std::filesystem::path& executable,
                const std::vector<std::string>& args) {
        if (pipe(m_engine_stdin_pipe) < 0 || pipe(m_engine_stdout_pipe) < 0) {
            throw std::runtime_error("Pipe creation failed");
        }

        m_pid = fork();
        if (m_pid < 0) {
            throw std::runtime_error("Fork failed");
        }

        if (m_pid == 0) {
            dup2(m_engine_stdin_pipe[0], STDIN_FILENO);
            dup2(m_engine_stdout_pipe[1], STDOUT_FILENO);
            dup2(m_engine_stdout_pipe[1], STDERR_FILENO);

            close(m_engine_stdin_pipe[0]);
            close(m_engine_stdin_pipe[1]);
            close(m_engine_stdout_pipe[0]);
            close(m_engine_stdout_pipe[1]);

            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>(executable.c_str()));
            for (const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);

            execvp(executable.c_str(), c_args.data());
            perror("execvp");
            exit(1);
        } else {
            close(m_engine_stdin_pipe[0]);
            close(m_engine_stdout_pipe[1]);
            m_engine_stdin_write_fd = m_engine_stdin_pipe[1];
            m_engine_stdout_read_fd = m_engine_stdout_pipe[0];
        }
    }

    ~ProcessImpl() {
        if (m_pid > 0) {
            terminate();
        }
        close(m_engine_stdin_write_fd);
        close(m_engine_stdout_read_fd);
    }

    bool is_running() const {
        if (m_pid <= 0) {
            return false;
        }
        int status;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
        return result == 0;
    }

    bool write_line(std::string_view line) {
        std::string full_line = std::string(line) + "\n";
        ssize_t bytes_written =
            write(m_engine_stdin_write_fd, full_line.c_str(), full_line.length());
        return bytes_written == static_cast<ssize_t>(full_line.length());
    }

    std::optional<std::string> read_line() {
        struct pollfd pfd = {m_engine_stdout_read_fd, POLLIN, 0};
        if (poll(&pfd, 1, 0) > 0) {
            char buffer[4096];
            ssize_t bytes_read = read(m_engine_stdout_read_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                m_buffer += buffer;
            }
        }

        if (auto pos = m_buffer.find('\n'); pos != std::string::npos) {
            std::string line = m_buffer.substr(0, pos);
            m_buffer.erase(0, pos + 1);
            return line;
        }
        return std::nullopt;
    }

    void terminate() {
        if (m_pid > 0) {
            kill(m_pid, SIGTERM);
            int status;
            waitpid(m_pid, &status, 0);
            m_pid = -1;
        }
    }

private:
    pid_t m_pid{-1};
    int m_engine_stdin_pipe[2]{};
    int m_engine_stdout_pipe[2]{};
    int m_engine_stdin_write_fd{-1};
    int m_engine_stdout_read_fd{-1};
    std::string m_buffer;
};

Process::Process(const std::filesystem::path& executable,
                 const std::vector<std::string>& args)
        : p_impl(std::make_unique<ProcessImpl>(executable, args)) {}
Process::~Process() = default;
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

bool Process::is_running() const { return p_impl->is_running(); }
bool Process::write_line(std::string_view line) { return p_impl->write_line(line); }
void Process::terminate() { p_impl->terminate(); }

std::optional<std::string> Process::read_line() { return p_impl->read_line(); }

} // namespace vgce::process
