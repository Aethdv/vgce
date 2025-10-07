#pragma once

#include "ftxui/component/screen_interactive.hpp"
#include "model/search_tree.hpp"
#include "uci/uci_client.hpp"
#include <chrono>
#include <fstream>
#include <mutex>

namespace vgce::tui {
class Renderer;
}

namespace vgce::core {

struct AppConfig {
    std::filesystem::path engine_path;
    std::string position_fen = "startpos";

    i32 eval_threshold = 30;
    u16 pv_depth_limit = 20;
    u16 multi_pv = 1;
    u16 max_depth = 0;

    bool enable_logging = true;
    bool show_help = false;
    bool pause_on_start = false;

    std::vector<std::string> custom_uci_options;
};

class Application {
public:
    Application();
    ~Application();
    
    i32 run(i32 argc, char* argv[]);
    void shutdown();
    void toggle_pause();
    void clear_tree();
    void export_tree();
    bool is_paused() const;

private:
    void uci_processing_loop();
    void setup_signal_handlers();
    void parse_arguments(i32 argc, char* argv[]);
    void print_usage(const char* program_name);
    void print_help();
    void send_position();
    void send_uci_options();
    void start_search();
    void stop_search();

    std::unique_ptr<process::Process> m_process;
    std::unique_ptr<uci::UciClient> m_uci_client;
    model::SearchTree m_search_tree;
    uci::GlobalStats m_global_stats;

    ftxui::ScreenInteractive m_screen = ftxui::ScreenInteractive::Fullscreen();
    std::unique_ptr<tui::Renderer> m_renderer;

    std::ofstream m_log_file;
    std::mutex m_log_mutex;

    std::atomic<bool> m_is_shutting_down{false};
    std::atomic<bool> m_is_paused{false};
    std::chrono::steady_clock::time_point m_search_start_time;
    
    AppConfig m_config;
};

} // namespace vgce::core
