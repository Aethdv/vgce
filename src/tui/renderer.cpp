#include "tui/renderer.hpp"
#include "core/application.hpp"
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <iomanip>
#include <sstream>

namespace vgce::tui {

using namespace ftxui;

namespace {

constexpr i32 BLUNDER_THRESHOLD_CP = 200;
constexpr i32 MISTAKE_THRESHOLD_CP = 100;
constexpr i32 INACCURACY_THRESHOLD_CP = 50;
constexpr i32 GOOD_THRESHOLD_CP = 30;
constexpr i32 BRILLIANT_THRESHOLD_CP = 150;

constexpr u64 VISIT_COUNT_THRESHOLD = 10;
constexpr u64 VISIT_RATIO_DIVISOR = 20;
constexpr u16 QSEARCH_DEPTH_THRESHOLD = 3;

std::string format_large_number(u64 num) {
    if (num >= 1'000'000'000) {
        return std::to_string(num / 1'000'000'000) + "." + 
               std::to_string((num / 100'000'000) % 10) + "B";
    } else if (num >= 1'000'000) {
        return std::to_string(num / 1'000'000) + "." + 
               std::to_string((num / 100'000) % 10) + "M";
    } else if (num >= 1'000) {
        return std::to_string(num / 1'000) + "." + 
               std::to_string((num / 100) % 10) + "K";
    }
    return std::to_string(num);
}

} // namespace

Renderer::Renderer(model::SearchTree& tree, uci::GlobalStats& stats,
                   ScreenInteractive& screen, const vgce::core::AppConfig& config,
                   std::chrono::steady_clock::time_point& search_start,
                   vgce::core::Application& app)
    : m_search_tree(tree), m_global_stats(stats), m_screen(screen), 
      m_config(config), m_search_start_time(search_start), m_app(app) {
}

void Renderer::start() {
    auto ui = build_ui();
    m_screen.Loop(ui);
}

std::string Renderer::format_elapsed_time() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_search_start_time).count();
    
    u64 hours = elapsed / 3600000;
    u64 minutes = (elapsed % 3600000) / 60000;
    u64 seconds = (elapsed % 60000) / 1000;
    
    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h " << minutes << "m " << seconds << "s";
    } else if (minutes > 0) {
        ss << minutes << "m " << seconds << "s";
    } else {
        ss << seconds << "." << (elapsed % 1000) / 100 << "s";
    }
    return ss.str();
}

std::string Renderer::format_score(const uci::Score& score) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (score.type == uci::Score::Type::Centipawns) {
        f64 pawns = static_cast<f64>(score.value) / 100.0;
        ss << std::showpos << pawns;
    } else {
        ss << "M" << std::showpos << score.value;
    }
    return ss.str();
}

Color Renderer::get_eval_color(i32 cp_score) const {
    if (cp_score > m_config.eval_threshold) {
        return Color::GreenLight;
    } else if (cp_score < -m_config.eval_threshold) {
        return Color::RedLight;
    }
    return Color::Default;
}

std::string Renderer::get_move_annotation(const model::SearchTree::Node* node, 
                                          const model::SearchTree::Node* parent) const {
    if (!node || !parent || !node->has_score() || !parent->has_score()) {
        return "";
    }
    
    i32 node_score = node->get_score_cp();
    i32 parent_score = parent->get_score_cp();
    i32 delta = parent_score - node_score;
    
    if (std::abs(delta) < INACCURACY_THRESHOLD_CP) {
        if (delta < -BRILLIANT_THRESHOLD_CP) {
            return "!!";
        } else if (delta < -GOOD_THRESHOLD_CP) {
            return "!";
        }
        return "";
    }
    
    if (delta >= BLUNDER_THRESHOLD_CP) {
        return "??";
    } else if (delta >= MISTAKE_THRESHOLD_CP) {
        return "?";
    } else if (delta >= INACCURACY_THRESHOLD_CP) {
        return "?!";
    } else if (delta <= -BRILLIANT_THRESHOLD_CP) {
        return "!!";
    } else if (delta <= -GOOD_THRESHOLD_CP) {
        return "!";
    }
    
    return "";
}

Element Renderer::render_header() {
    std::string static_eval_str = "N/A";
    if (m_global_stats.static_eval) {
        static_eval_str = format_score(*m_global_stats.static_eval);
    }

    std::string wdl_str = "";
    if (m_global_stats.wdl_stats) {
        const auto& wdl = *m_global_stats.wdl_stats;
        f64 total = wdl.win + wdl.draw + wdl.loss;
        if (total > 0) {
            u16 win_pct = static_cast<u16>((wdl.win / total) * 100);
            u16 draw_pct = static_cast<u16>((wdl.draw / total) * 100);
            u16 loss_pct = static_cast<u16>((wdl.loss / total) * 100);
            wdl_str = " W:" + std::to_string(win_pct) + "% D:" + std::to_string(draw_pct) + 
                      "% L:" + std::to_string(loss_pct) + "%";
        }
    }

    auto title = text(" VGCE v0.1.0") | bold | color(Color::CyanLight);
    auto engine_text = text(" Engine: " + m_global_stats.engine_name) | color(Color::GrayLight);
    
    if (m_app.is_paused()) {
        engine_text = hbox({engine_text, text(" "), text("[PAUSED]") | color(Color::YellowLight) | bold});
    }
    
    auto nodes_str = format_large_number(m_global_stats.nodes.load());
    auto nps_str = format_large_number(m_global_stats.nps.load());
    
    Elements stats_line1;
    stats_line1.push_back(text("Nodes: ") | color(Color::GrayDark));
    stats_line1.push_back(text(nodes_str) | bold);
    stats_line1.push_back(text(" | NPS: ") | color(Color::GrayDark));
    stats_line1.push_back(text(nps_str) | bold);
    stats_line1.push_back(text(" | Time: ") | color(Color::GrayDark));
    stats_line1.push_back(text(format_elapsed_time()) | bold);
    
    Elements stats_line2;
    stats_line2.push_back(text("Hash: ") | color(Color::GrayDark));
    stats_line2.push_back(text(std::to_string(m_global_stats.hashfull.load() / 10) + "%") | bold);
    
    if (m_global_stats.tbhits.load() > 0) {
        stats_line2.push_back(text(" | TB Hits: ") | color(Color::GrayDark));
        stats_line2.push_back(text(format_large_number(m_global_stats.tbhits.load())) | bold);
    }
    
    stats_line2.push_back(text(" | Static Eval: ") | color(Color::GrayDark));
    auto eval_elem = text(static_eval_str) | bold;
    if (m_global_stats.static_eval) {
        eval_elem = eval_elem | color(get_eval_color(m_global_stats.static_eval->value));
    }
    stats_line2.push_back(eval_elem);
    
    if (!wdl_str.empty()) {
        stats_line2.push_back(text(" |") | color(Color::GrayDark));
        stats_line2.push_back(text(wdl_str) | color(Color::Yellow));
    }
    
    auto best_move = m_search_tree.get_best_move();
    Elements stats_line3;
    stats_line3.push_back(text("Best Move: ") | color(Color::GrayDark));
    stats_line3.push_back(text(best_move.empty() ? "..." : best_move) | bold | color(Color::GreenLight));
    
    if (!m_global_stats.current_move.empty()) {
        stats_line3.push_back(text(" | Current: ") | color(Color::GrayDark));
        stats_line3.push_back(text(m_global_stats.current_move) | color(Color::Cyan));
        if (m_global_stats.current_move_number > 0) {
            stats_line3.push_back(text(" (#" + std::to_string(m_global_stats.current_move_number) + ")") | 
                                 color(Color::GrayDark) | dim);
        }
    }
    
    if (m_config.multi_pv > 1) {
        stats_line3.push_back(text(" | MultiPV: ") | color(Color::GrayDark));
        stats_line3.push_back(text(std::to_string(m_config.multi_pv)) | bold);
    }
    
    stats_line3.push_back(text(" | Tree Nodes: ") | color(Color::GrayDark));
    stats_line3.push_back(text(std::to_string(m_search_tree.get_total_nodes())) | bold);

    return vbox({
        hbox({title, text(" | "), engine_text}),
        separator(),
        hbox(stats_line1),
        hbox(stats_line2),
        hbox(stats_line3),
        separator(),
    }) | border;
}

void Renderer::render_tree_node(const model::SearchTree::Node* node, Elements& elements,
                                const std::string& prefix, bool is_last, u16 current_depth,
                                u16 ply_number, bool white_to_move) {
    if (!node || current_depth > m_config.pv_depth_limit) {
        return;
    }

    Elements line_elements;
    
    std::string branch = is_last ? "└─" : "├─";
    line_elements.push_back(text(prefix + branch + " ") | color(Color::GrayDark));
    
    if (white_to_move) {
        line_elements.push_back(text(std::to_string(ply_number) + ".") | color(Color::GrayLight));
    } else {
        line_elements.push_back(text(std::to_string(ply_number) + "..") | color(Color::GrayLight));
    }
    
    Color move_color = Color::Default;
    if (node->is_pv_node) {
        move_color = Color::GreenLight;
    } else if (node->multipv_index > 1) {
        move_color = Color::Cyan;
    }
    
    line_elements.push_back(text(node->move) | color(move_color) | bold);
    
    std::string annotation = get_move_annotation(node, node->parent);
    if (!annotation.empty()) {
        Color annot_color = Color::Yellow;
        if (annotation == "!!" || annotation == "!") {
            annot_color = Color::GreenLight;
        } else if (annotation == "??" || annotation == "?") {
            annot_color = Color::RedLight;
        }
        line_elements.push_back(text(annotation) | color(annot_color) | bold);
    }

    if (node->data.depth) {
        std::stringstream info;
        info << " (d" << *node->data.depth;
        if (node->data.seldepth && *node->data.seldepth > *node->data.depth) {
            info << "/" << *node->data.seldepth;
        }
        line_elements.push_back(text(info.str()) | color(Color::GrayDark));

        if (node->data.score) {
            line_elements.push_back(text(" ") | color(Color::GrayDark));
            auto score_text = text(format_score(*node->data.score)) | bold;
            score_text = score_text | color(get_eval_color(node->get_score_cp()));
            line_elements.push_back(score_text);
        }
        
        line_elements.push_back(text(")") | color(Color::GrayDark));
        
        if (node->data.seldepth && node->data.depth && 
            *node->data.seldepth > *node->data.depth + QSEARCH_DEPTH_THRESHOLD) {
            std::stringstream qsearch;
            qsearch << " [Q+" << (*node->data.seldepth - *node->data.depth) << "]";
            line_elements.push_back(text(qsearch.str()) | color(Color::Cyan) | dim);
        }
    }
    
    if (node->visit_count > VISIT_COUNT_THRESHOLD) {
        line_elements.push_back(text(" [TT×" + std::to_string(node->visit_count) + "]") | 
                               color(Color::Yellow) | dim);
    }
    
    if (node->multipv_index > 1) {
        line_elements.push_back(text(" {PV" + std::to_string(node->multipv_index) + "}") | 
                               color(Color::Cyan) | dim);
    }

    elements.push_back(hbox(line_elements));

    const std::string child_prefix = prefix + (is_last ? "  " : "│ ");
    const auto& children = node->children;
    
    auto it = children.begin();
    while (it != children.end()) {
        bool is_child_last = (std::next(it) == children.end());
        u16 next_ply = white_to_move ? ply_number : ply_number + 1;
        render_tree_node(it->second.get(), elements, child_prefix, is_child_last,
                        current_depth + 1, next_ply, !white_to_move);
        ++it;
    }
}

Element Renderer::render_tree_view() {
    Elements elements;
    const auto* root = m_search_tree.get_root();
    if (!root) {
        return text("Initializing search...") | center | color(Color::GrayLight);
    }

    const auto& children = root->children;
    if (children.empty()) {
        if (m_app.is_paused()) {
            return text("Search is paused. Press Space to resume.") | center | color(Color::YellowLight);
        }
        return text("Waiting for engine output...") | center | color(Color::GrayLight);
    }
    
    auto it = children.begin();
    while (it != children.end()) {
        bool is_last = (std::next(it) == children.end());
        render_tree_node(it->second.get(), elements, "", is_last, 1, 1, true);
        ++it;
    }
    
    return vbox(elements);
}

Element Renderer::render_footer() {
    Elements help_elements;
    help_elements.push_back(text(" Controls: ") | bold | color(Color::GrayLight));
    help_elements.push_back(text("Mouse Wheel") | color(Color::CyanLight));
    help_elements.push_back(text(" Scroll ") | color(Color::GrayDark));
    help_elements.push_back(text("Space") | color(Color::YellowLight));
    help_elements.push_back(text(" Pause ") | color(Color::GrayDark));
    help_elements.push_back(text("c") | color(Color::MagentaLight));
    help_elements.push_back(text(" Clear ") | color(Color::GrayDark));
    help_elements.push_back(text("e") | color(Color::GreenLight));
    help_elements.push_back(text(" Export ") | color(Color::GrayDark));
    help_elements.push_back(text("q") | color(Color::RedLight));
    help_elements.push_back(text(" Quit") | color(Color::GrayDark));
    
    return hbox(help_elements) | border;
}

Component Renderer::build_ui() {
    auto header = ftxui::Renderer([this] { return render_header(); });
    auto footer = ftxui::Renderer([this] { return render_footer(); });
    
    auto tree_container = Container::Vertical({});
    
    auto tree_with_scroll = ftxui::Renderer(tree_container, [this] {
        return render_tree_view() | vscroll_indicator | yframe | flex;
    });
    
    Components children;
    children.push_back(header);
    children.push_back(tree_with_scroll);
    children.push_back(footer);
    
    auto main_container = Container::Vertical(children);
    
    auto component = CatchEvent(main_container, [this](Event event) {
        if (event == Event::Character('q')) {
            m_screen.ExitLoopClosure()();
            return true;
        } else if (event == Event::Character(' ')) {
            m_app.toggle_pause();
            return true;
        } else if (event == Event::Character('c')) {
            m_app.clear_tree();
            return true;
        } else if (event == Event::Character('e')) {
            m_app.export_tree();
            return true;
        }
        return false;
    });
    
    return component;
}

} // namespace vgce::tui
