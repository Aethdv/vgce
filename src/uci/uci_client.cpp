#include "uci/uci_client.hpp"
#include <chrono>

namespace vgce::uci {

UciClient::UciClient(std::unique_ptr<process::Process> engine_process)
    : m_process(std::move(engine_process)) {
}

UciClient::~UciClient() {
    if (m_is_running.load()) {
        stop();
    }
}

void UciClient::start() {
    m_is_running.store(true);
    m_reader_thread = std::thread(&UciClient::reader_loop, this);
}

void UciClient::stop() {
    m_is_running.store(false);
    if (m_reader_thread.joinable()) {
        m_reader_thread.join();
    }
    m_process->terminate();
}

void UciClient::send_command(std::string_view command) {
    m_process->write_line(command);
}

ConcurrentQueue<std::string>& UciClient::get_output_queue() {
    return m_output_queue;
}

void UciClient::reader_loop() {
    while (m_is_running.load()) {
        if (auto line = m_process->read_line()) {
            m_output_queue.push(std::move(*line));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!m_process->is_running()) {
            m_is_running.store(false);
        }
    }
}

} // namespace vgce::uci
