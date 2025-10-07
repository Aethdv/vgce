#pragma once

#include "core/application.hpp"
#include "ftxui/component/component.hpp"
#include "model/search_tree.hpp"
#include "uci/uci_data.hpp"
#include <atomic>
#include <chrono>
#include <mutex>

namespace vgce::core {
struct AppConfig;
class Application;
}

namespace vgce::tui {

class Renderer {
public:
    Renderer(model::SearchTree& tree, uci::GlobalStats& stats, 
             ftxui::ScreenInteractive& screen, const vgce::core::AppConfig& config,
             std::chrono::steady_clock::time_point& search_start,
             vgce::core::Application& app);

    void start();

private:
    ftxui::Component build_ui();
    ftxui::Element render_header();
    ftxui::Element render_tree_view();
    ftxui::Element render_footer();
    
    void render_tree_node(const model::SearchTree::Node* node, ftxui::Elements& elements,
                          const std::string& prefix, bool is_last, u16 current_depth,
                          u16 ply_number, bool white_to_move);
    
    std::string get_move_annotation(const model::SearchTree::Node* node, 
                                    const model::SearchTree::Node* parent) const;
    ftxui::Color get_eval_color(i32 cp_score) const;
    std::string format_elapsed_time() const;
    std::string format_score(const uci::Score& score) const;

    model::SearchTree& m_search_tree;
    uci::GlobalStats& m_global_stats;
    ftxui::ScreenInteractive& m_screen;
    const vgce::core::AppConfig& m_config;
    std::chrono::steady_clock::time_point& m_search_start_time;
    vgce::core::Application& m_app;
};

} // namespace vgce::tui
