#pragma once

#include "concurrent_queue.hpp"
#include "process/process.hpp"
#include "types.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace vgce::uci {

class UciClient {
public:
    explicit UciClient(std::unique_ptr<process::Process> engine_process);
    ~UciClient();

    UciClient(const UciClient&) = delete;
    UciClient& operator=(const UciClient&) = delete;

    void start();
    void stop();

    void send_command(std::string_view command);

    ConcurrentQueue<std::string>& get_output_queue();

private:
    void reader_loop();

    std::unique_ptr<process::Process> m_process;
    std::thread m_reader_thread;
    ConcurrentQueue<std::string> m_output_queue;
    std::atomic<bool> m_is_running{false};
};

} // namespace vgce::uci
