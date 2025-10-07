#include "model/search_tree.hpp"
#include <iomanip>
#include <sstream>
#include <functional>

namespace vgce::model {

SearchTree::SearchTree() {
    m_root = std::make_unique<Node>();
    m_root->move = "root";
}

i32 SearchTree::Node::get_score_cp() const {
    if (!data.score) {
        return 0;
    }
    if (data.score->type == uci::Score::Type::Centipawns) {
        return data.score->value;
    }
    return data.score->value > 0 ? 10000 : -10000;
}

bool SearchTree::Node::has_score() const {
    return data.score.has_value();
}

void SearchTree::clear_pv_flags(Node* node) {
    if (!node) {
        return;
    }
    node->is_pv_node = false;
    for (auto const& [key, val] : node->children) {
        clear_pv_flags(val.get());
    }
}

void SearchTree::clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_root = std::make_unique<Node>();
    m_root->move = "root";
}

void SearchTree::update(const uci::InfoData& data) {
    if (data.pv.empty()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (!data.multipv || *data.multipv == 1) {
        clear_pv_flags(m_root.get());
    }

    Node* current_node = m_root.get();
    for (const auto& move_str : data.pv) {
        auto it = current_node->children.find(move_str);
        if (it == current_node->children.end()) {
            auto new_node = std::make_unique<Node>();
            
            new_node->move = move_str;
            new_node->parent = current_node;
            it = current_node->children.emplace(move_str, std::move(new_node)).first;
        }
        current_node = it->second.get();
        
        if (!data.multipv || *data.multipv == 1) {
            current_node->is_pv_node = true;
        }
        
        if (data.multipv) {
            current_node->multipv_index = *data.multipv;
        }
        
        current_node->visit_count++;
    }
    current_node->data = data;
}

std::string SearchTree::get_best_move() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    if (m_root->children.empty()) {
        return "";
    }
    
    for (const auto& [move, node] : m_root->children) {
        if (node->is_pv_node) {
            return move;
        }
    }
    
    return m_root->children.begin()->first;
}

u64 SearchTree::get_total_nodes() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    u64 count = 0;
    
    std::function<void(const Node*)> count_nodes = [&](const Node* node) {
        if (!node) return;
        count++;
        for (const auto& [move, child] : node->children) {
            count_nodes(child.get());
        }
    };
    
    count_nodes(m_root.get());
    return count > 0 ? count - 1 : 0;
}

void SearchTree::export_node(const Node* node, std::stringstream& ss, const std::string& prefix, bool is_last, u16 depth) const {
    if (!node || depth > 100) {
        return;
    }

    ss << prefix << (is_last ? "└── " : "├── ") << node->move;
    
    if (node->data.depth) {
        ss << " (d" << *node->data.depth;
        if (node->data.seldepth) {
            ss << "/" << *node->data.seldepth;
        }
        if (node->data.score) {
            ss << ", ";
            if (node->data.score->type == uci::Score::Type::Centipawns) {
                ss << std::fixed << std::setprecision(2) << (static_cast<f64>(node->data.score->value) / 100.0);
            } else {
                ss << "M" << node->data.score->value;
            }
        }
        ss << ")";
    }
    
    if (node->visit_count > 1) {
        ss << " [TT×" << node->visit_count << "]";
    }
    
    ss << "\n";

    const std::string child_prefix = prefix + (is_last ? "    " : "│   ");
    const auto& children = node->children;
    
    auto it = children.begin();
    while (it != children.end()) {
        bool is_child_last = (std::next(it) == children.end());
        export_node(it->second.get(), ss, child_prefix, is_child_last, depth + 1);
        ++it;
    }
}

std::string SearchTree::export_to_string() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::stringstream ss;
    
    ss << "Search Tree:\n";
    const auto& children = m_root->children;
    auto it = children.begin();
    while (it != children.end()) {
        bool is_last = (std::next(it) == children.end());
        export_node(it->second.get(), ss, "", is_last, 1);
        ++it;
    }
    
    return ss.str();
}

const SearchTree::Node* SearchTree::get_root() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_root.get();
}

} // namespace vgce::model
