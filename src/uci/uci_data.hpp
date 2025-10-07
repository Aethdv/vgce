#pragma once

#include "types.hpp"
#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace vgce::uci {

struct Score {
    enum class Type { Centipawns, Mate };
    Type type;
    i32 value;
};

struct WDL {
    u32 win;
    u32 draw;
    u32 loss;
};

struct GlobalStats {
    std::atomic<u64> nodes{0};
    std::atomic<u32> nps{0};
    std::atomic<u16> hashfull{0};
    std::atomic<u32> tbhits{0};
    std::atomic<u64> time_ms{0};
    std::optional<Score> static_eval;
    std::optional<WDL> wdl_stats;
    std::string engine_name;
    u16 current_multipv{1};
    std::string current_move;
    u16 current_move_number{0};
};

struct InfoData {
    std::optional<u16> depth;
    std::optional<u16> seldepth;
    std::optional<Score> score;
    std::optional<u64> nodes;
    std::optional<u32> nps;
    std::optional<u32> tbhits;
    std::optional<u16> hashfull;
    std::optional<u64> time;
    std::optional<u16> multipv;
    std::vector<std::string> pv;

    std::optional<WDL> wdl;
    std::optional<Score> static_eval;
    std::string raw_string;
    std::string currmove;
    std::optional<u16> currmovenumber;
};

} // namespace vgce::uci
