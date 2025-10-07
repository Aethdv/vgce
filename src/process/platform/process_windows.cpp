#include "process/process.hpp"
#include <windows.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace vgce::process {

class Process::ProcessImpl {
public:
    ProcessImpl(const std::filesystem::path& executable,
                const std::vector<std::string>& args) {
        SECURITY_ATTRIBUTES sa_attr;
        sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa_attr.bInheritHandle = TRUE;
        sa_attr.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&m_engine_stdin_read, &m_engine_stdin_write, &sa_attr, 0)) {
            throw std::runtime_error("Failed to create engine stdin pipe");
        }
        if (!SetHandleInformation(m_engine_stdin_write, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error("Failed to set handle information for stdin");
        }

        if (!CreatePipe(&m_engine_stdout_read, &m_engine_stdout_write, &sa_attr, 0)) {
            throw std::runtime_error("Failed to create engine stdout pipe");
        }
        if (!SetHandleInformation(m_engine_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error("Failed to set handle information for stdout");
        }

        STARTUPINFOW si_startup_info{};
        si_startup_info.cb = sizeof(STARTUPINFOW);
        si_startup_info.hStdError = m_engine_stdout_write;
        si_startup_info.hStdOutput = m_engine_stdout_write;
        si_startup_info.hStdInput = m_engine_stdin_read;
        si_startup_info.dwFlags |= STARTF_USESTDHANDLES;

        std::string command_line = "\"" + executable.string() + "\"";
        for (const auto& arg : args) {
            command_line += " " + arg;
        }
        std::wstring w_command_line(command_line.begin(), command_line.end());

        if (!CreateProcessW(nullptr, &w_command_line[0], nullptr, nullptr, TRUE, 0,
                            nullptr, nullptr, &si_startup_info, &m_process_info)) {
            throw std::runtime_error("Failed to create process");
        }

        CloseHandle(m_engine_stdin_read);
        CloseHandle(m_engine_stdout_write);
    }

    ~ProcessImpl() {
        if (m_is_running) {
            terminate();
        }
        CloseHandle(m_engine_stdin_write);
        CloseHandle(m_engine_stdout_read);
    }

    bool is_running() const {
        if (!m_is_running) {
            return false;
        }
        DWORD exit_code;
        GetExitCodeProcess(m_process_info.hProcess, &exit_code);
        return exit_code == STILL_ACTIVE;
    }

    bool write_line(std::string_view line) {
        std::string full_line = std::string(line) + "\n";
        DWORD bytes_written;
        return WriteFile(m_engine_stdin_write, full_line.c_str(),
                         static_cast<DWORD>(full_line.length()), &bytes_written, nullptr);
    }

    std::optional<std::string> read_line() {
        char buffer[4096];
        DWORD bytes_read;
        if (ReadFile(m_engine_stdout_read, buffer, sizeof(buffer) - 1, &bytes_read,
                     nullptr) &&
            bytes_read > 0) {
            buffer[bytes_read] = '\0';
            m_buffer += buffer;
        }

        if (auto pos = m_buffer.find('\n'); pos != std::string::npos) {
            std::string line = m_buffer.substr(0, pos);
            m_buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        return std::nullopt;
    }

    void terminate() {
        if (m_is_running) {
            TerminateProcess(m_process_info.hProcess, 0);
            CloseHandle(m_process_info.hProcess);
            CloseHandle(m_process_info.hThread);
            m_is_running = false;
        }
    }

private:
    PROCESS_INFORMATION m_process_info{};
    HANDLE m_engine_stdin_read{nullptr}, m_engine_stdin_write{nullptr};
    HANDLE m_engine_stdout_read{nullptr}, m_engine_stdout_write{nullptr};
    std::string m_buffer;
    bool m_is_running{true};
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
