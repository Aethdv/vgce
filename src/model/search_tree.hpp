#pragma once

#include "uci/uci_data.hpp"
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <mutex>

namespace vgce::model {

class SearchTree {
public:
    struct Node {
        std::string move;
        uci::InfoData data;
        u64 visit_count = 0;
        u16 multipv_index = 0;
        bool is_pv_node = false;
        std::map<std::string, std::unique_ptr<Node>> children;
        Node* parent = nullptr;
        
        i32 get_score_cp() const;
        bool has_score() const;
    };

public:
    SearchTree();

    void update(const uci::InfoData& data);
    void clear();
    const Node* get_root() const;
    std::string get_best_move() const;
    std::string export_to_string() const;
    
    u64 get_total_nodes() const;

private:
    void clear_pv_flags(Node* node);
    void export_node(const Node* node, std::stringstream& ss, const std::string& prefix, bool is_last, u16 depth) const;

    std::unique_ptr<Node> m_root;
    mutable std::shared_mutex m_mutex;
};

} // namespace vgce::model
