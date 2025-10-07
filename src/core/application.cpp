#include "core/application.hpp"
#include "tui/renderer.hpp"
#include "uci/uci_parser.hpp"
#include <csignal>
#include <iostream>
#include <sstream>
#include <thread>

namespace vgce::core {

namespace {
Application* g_app_instance = nullptr;

void signal_handler(i32) {
    if (g_app_instance) {
        g_app_instance->shutdown();
    }
}
} // namespace

Application::Application() {
    g_app_instance = this;
}

Application::~Application() {
    g_app_instance = nullptr;
    if (m_log_file.is_open()) {
        m_log_file.close();
    }
}

void Application::setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void Application::shutdown() {
    m_is_shutting_down.store(true);
    m_screen.PostEvent(ftxui::Event::Custom);
}

void Application::toggle_pause() {
    bool was_paused = m_is_paused.load();
    m_is_paused.store(!was_paused);
    
    if (was_paused) {
        start_search();
    } else {
        stop_search();
    }
    m_screen.PostEvent(ftxui::Event::Custom);
}

void Application::clear_tree() {
    m_search_tree.clear();
    m_global_stats.nodes.store(0);
    m_global_stats.nps.store(0);
    m_global_stats.hashfull.store(0);
    m_global_stats.tbhits.store(0);
    m_global_stats.time_ms.store(0);
    m_screen.PostEvent(ftxui::Event::Custom);
}

void Application::export_tree() {
    std::string filename = "vgce_tree_export_" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".txt";
    
    std::ofstream export_file(filename);
    if (export_file.is_open()) {
        export_file << "VGCE Tree Export\n";
        export_file << "================\n\n";
        export_file << "Engine: " << m_global_stats.engine_name << "\n";
        export_file << "Position: " << m_config.position_fen << "\n";
        export_file << "Nodes: " << m_global_stats.nodes.load() << "\n";
        export_file << "Time: " << m_global_stats.time_ms.load() << "ms\n\n";
        export_file << m_search_tree.export_to_string();
        export_file.close();
    }
}

bool Application::is_paused() const {
    return m_is_paused.load();
}

void Application::print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <engine_executable> [options]\n"
              << "Try '" << program_name << " -h' for more information.\n";
}

void Application::print_help() {
    std::cout << R"(
VGCE - Visual Game-tree Chess Engine Explorer v0.1.0

USAGE:
    vgce <engine_executable> [OPTIONS]

ARGUMENTS:
    <engine_executable>    Path to UCI chess engine executable

OPTIONS:
    -h, --help                     Show this help message
    --position <fen>               Set starting position (default: startpos)
                                   Use 'startpos' or a valid FEN string
    
    --pv-depth <depth>             Maximum PV depth to display (default: 20)
                                   Range: 1-100
    
    --multi-pv <count>             Number of principal variations (default: 1)
                                   Range: 1-256
    
    --max-depth <depth>            Maximum search depth (default: infinite)
                                   Limits engine search depth
    
    --eval-threshold <cp>          Eval difference threshold for highlighting (default: 30)
                                   Centipawns to consider a move significant
    
    --pause                        Start with search paused
    
    --no-log                       Disable engine output logging
    
    --uci-option <name>=<value>    Send custom UCI option to engine
                                   Can be specified multiple times
                                   Example: --uci-option Hash=2048

INTERACTIVE CONTROLS:
    Arrow Up/Down       Scroll through the search tree
    Page Up/Down        Scroll faster (5 lines)
    Home/End            Jump to top/bottom
    Space               Pause/Resume search
    c                   Clear tree and restart
    e                   Export tree to text file
    q, Ctrl+C           Quit application

EXAMPLES:
    # Basic usage with Stockfish
    ./vgce stockfish
    
    # Analyze a specific position with MultiPV
    ./vgce stockfish --position "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3" --multi-pv 3
    
    # Limit tree depth and set hash size
    ./vgce stockfish --pv-depth 15 --max-depth 30 --uci-option Hash=4096
    
    # Disable logging for performance
    ./vgce lc0 --no-log --multi-pv 5
    
    # Start paused for manual control
    ./vgce stockfish --pause --pv-depth 25

COLOR GUIDE:
    Green               PV (Principal Variation) moves
    Cyan                Alternative MultiPV lines
    Red/Green           Evaluation scores (bad/good)
    Yellow              Transposition table hits, WDL stats
    Gray                Metadata and structural elements

ANNOTATIONS:
    !!                  Brilliant move (eval improvement > 150cp)
    !                   Good move (eval improvement > 30cp)
    ?!                  Dubious move (eval loss 50-100cp)
    ?                   Mistake (eval loss 100-200cp)
    ??                  Blunder (eval loss > 200cp)

AUTHOR:
    Aethdv

REPORTING BUGS:
    Please report bugs and feature requests to:
    https://github.com/Aethdv/vgce
)";
}

void Application::parse_arguments(i32 argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        throw std::runtime_error("Insufficient arguments");
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        m_config.show_help = true;
        return;
    }

    m_config.engine_path = argv[1];

    for (i32 i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            m_config.show_help = true;
            return;
        } else if (arg == "--position" && i + 1 < argc) {
            m_config.position_fen = argv[++i];
        } else if (arg == "--pv-depth" && i + 1 < argc) {
            i32 depth = std::atoi(argv[++i]);
            if (depth > 0 && depth <= 100) {
                m_config.pv_depth_limit = static_cast<u16>(depth);
            } else {
                std::cerr << "Warning: Invalid PV depth '" << depth << "', using default (20)\n";
            }
        } else if (arg == "--multi-pv" && i + 1 < argc) {
            i32 multipv = std::atoi(argv[++i]);
            if (multipv > 0 && multipv <= 256) {
                m_config.multi_pv = static_cast<u16>(multipv);
            } else {
                std::cerr << "Warning: Invalid MultiPV count, using default (1)\n";
            }
        } else if (arg == "--max-depth" && i + 1 < argc) {
            i32 depth = std::atoi(argv[++i]);
            if (depth > 0) {
                m_config.max_depth = static_cast<u16>(depth);
            }
        } else if (arg == "--eval-threshold" && i + 1 < argc) {
            i32 threshold = std::atoi(argv[++i]);
            if (threshold > 0) {
                m_config.eval_threshold = threshold;
            }
        } else if (arg == "--pause") {
            m_config.pause_on_start = true;
        } else if (arg == "--no-log") {
            m_config.enable_logging = false;
        } else if (arg == "--uci-option" && i + 1 < argc) {
            m_config.custom_uci_options.push_back(argv[++i]);
        } else {
            std::cerr << "Warning: Unknown argument '" << arg << "'\n";
        }
    }
}

void Application::send_uci_options() {
    if (m_config.multi_pv > 1) {
        m_uci_client->send_command("setoption name MultiPV value " + std::to_string(m_config.multi_pv));
    }
    
    for (const auto& option : m_config.custom_uci_options) {
        auto pos = option.find('=');
        if (pos != std::string::npos) {
            std::string name = option.substr(0, pos);
            std::string value = option.substr(pos + 1);
            m_uci_client->send_command("setoption name " + name + " value " + value);
        }
    }
}

void Application::send_position() {
    if (m_config.position_fen == "startpos") {
        m_uci_client->send_command("position startpos");
    } else {
        m_uci_client->send_command("position fen " + m_config.position_fen);
    }
}

void Application::start_search() {
    m_search_start_time = std::chrono::steady_clock::now();
    
    std::string go_cmd = "go";
    if (m_config.max_depth > 0) {
        go_cmd += " depth " + std::to_string(m_config.max_depth);
    } else {
        go_cmd += " infinite";
    }
    
    m_uci_client->send_command(go_cmd);
}

void Application::stop_search() {
    m_uci_client->send_command("stop");
}

i32 Application::run(i32 argc, char* argv[]) {
    try {
        parse_arguments(argc, argv);
    } catch (const std::exception& e) {
        return 1;
    }

    if (m_config.show_help) {
        print_help();
        return 0;
    }

    setup_signal_handlers();

    if (m_config.enable_logging) {
        m_log_file.open("vgce_engine_log.txt", std::ios::out | std::ios::trunc);
    }

    if (m_config.pause_on_start) {
        m_is_paused.store(true);
    }

    try {
        m_process = std::make_unique<process::Process>(m_config.engine_path, std::vector<std::string>{});
        m_uci_client = std::make_unique<uci::UciClient>(std::move(m_process));
        m_renderer = std::make_unique<tui::Renderer>(
            m_search_tree, m_global_stats, m_screen, m_config, m_search_start_time, *this);

        m_uci_client->start();
        std::thread uci_thread(&Application::uci_processing_loop, this);

        m_renderer->start();

        m_is_shutting_down.store(true);
        m_uci_client->stop();
        if (uci_thread.joinable()) {
            uci_thread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

void Application::uci_processing_loop() {
    m_uci_client->send_command("uci");
    bool uci_ok = false;
    
    while (!uci_ok && !m_is_shutting_down) {
        if (auto line = m_uci_client->get_output_queue().pop()) {
            constexpr u64 ID_NAME_PREFIX_LEN = 8;
            if (line->find("id name ") == 0 && line->length() > ID_NAME_PREFIX_LEN) {
                m_global_stats.engine_name = line->substr(ID_NAME_PREFIX_LEN);
            }
            if (*line == "uciok") {
                uci_ok = true;
            }
        }
    }
    
    m_uci_client->send_command("isready");
    send_uci_options();
    send_position();
    
    if (!m_config.pause_on_start) {
        start_search();
    }

    while (!m_is_shutting_down.load()) {
        auto line = m_uci_client->get_output_queue().pop();
        if (!line) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (m_log_file.is_open()) {
            std::lock_guard<std::mutex> lock(m_log_mutex);
            m_log_file << *line << std::endl;
        }

        auto info = uci::parse_line(*line);
        if (info) {
            if (info->nodes) {
                m_global_stats.nodes.store(*info->nodes);
            }
            if (info->nps) {
                m_global_stats.nps.store(*info->nps);
            }
            if (info->hashfull) {
                m_global_stats.hashfull.store(*info->hashfull);
            }
            if (info->tbhits) {
                m_global_stats.tbhits.store(*info->tbhits);
            }
            if (info->time) {
                m_global_stats.time_ms.store(*info->time);
            }
            if (info->wdl) {
                m_global_stats.wdl_stats = *info->wdl;
            }
            if (info->static_eval) {
                m_global_stats.static_eval = *info->static_eval;
            }
            if (info->multipv) {
                m_global_stats.current_multipv = *info->multipv;
            }
            if (!info->currmove.empty()) {
                m_global_stats.current_move = info->currmove;
            }
            if (info->currmovenumber) {
                m_global_stats.current_move_number = *info->currmovenumber;
            }

            if (!info->pv.empty()) {
                m_search_tree.update(*info);
            }
            m_screen.PostEvent(ftxui::Event::Custom);
        }
    }
}

} // namespace vgce::core
