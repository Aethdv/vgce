#pragma once

#include "types.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vgce::process {

class Process {
public:
    Process(const std::filesystem::path& executable, const std::vector<std::string>& args);
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;

    bool is_running() const;
    bool write_line(std::string_view line);
    std::optional<std::string> read_line();
    void terminate();

private:
    class ProcessImpl;
    std::unique_ptr<ProcessImpl> p_impl;
};

} // namespace vgce::process
