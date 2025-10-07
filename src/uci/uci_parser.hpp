#pragma once

#include "uci_data.hpp"
#include "types.hpp"
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vgce::uci {

std::optional<InfoData> parse_line(std::string_view line);

namespace detail {

std::optional<InfoData> parse_info_search(std::string_view line);
std::optional<InfoData> parse_info_string(std::string_view line);
std::vector<std::string_view> split(std::string_view str, char delimiter);
std::optional<Score> parse_score(std::string_view type, std::string_view value);

template <typename T>
std::optional<T> parse_unsigned(std::string_view sv) {
    T value{};
    auto result = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return (result.ec == std::errc()) ? std::optional(value) : std::nullopt;
}

template <typename T>
std::optional<T> parse_signed(std::string_view sv) {
    T value{};
    auto result = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return (result.ec == std::errc()) ? std::optional(value) : std::nullopt;
}

std::optional<f64> parse_float(std::string_view sv);

} // namespace detail

inline std::optional<InfoData> parse_line(std::string_view line) {
    if (line.starts_with("info string")) {
        return detail::parse_info_string(line);
    }
    if (line.starts_with("info")) {
        return detail::parse_info_search(line);
    }
    return std::nullopt;
}

namespace detail {

inline std::optional<InfoData> parse_info_search(std::string_view line) {
    InfoData data;
    data.raw_string = line;
    std::vector<std::string_view> tokens = split(line, ' ');

    for (u64 i = 1; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token == "depth" && i + 1 < tokens.size()) {
            data.depth = parse_unsigned<u16>(tokens[++i]);
        } else if (token == "seldepth" && i + 1 < tokens.size()) {
            data.seldepth = parse_unsigned<u16>(tokens[++i]);
        } else if (token == "score" && i + 2 < tokens.size()) {
            data.score = parse_score(tokens[i + 1], tokens[i + 2]);
            i += 2;
        } else if (token == "nodes" && i + 1 < tokens.size()) {
            data.nodes = parse_unsigned<u64>(tokens[++i]);
        } else if (token == "nps" && i + 1 < tokens.size()) {
            data.nps = parse_unsigned<u32>(tokens[++i]);
        } else if (token == "hashfull" && i + 1 < tokens.size()) {
            data.hashfull = parse_unsigned<u16>(tokens[++i]);
        } else if (token == "tbhits" && i + 1 < tokens.size()) {
            data.tbhits = parse_unsigned<u32>(tokens[++i]);
        } else if (token == "time" && i + 1 < tokens.size()) {
            data.time = parse_unsigned<u64>(tokens[++i]);
        } else if (token == "multipv" && i + 1 < tokens.size()) {
            data.multipv = parse_unsigned<u16>(tokens[++i]);
        } else if (token == "currmove" && i + 1 < tokens.size()) {
            data.currmove = std::string(tokens[++i]);
        } else if (token == "currmovenumber" && i + 1 < tokens.size()) {
            data.currmovenumber = parse_unsigned<u16>(tokens[++i]);
        } else if (token == "wdl" && i + 3 < tokens.size()) {
            auto w = parse_unsigned<u32>(tokens[i + 1]);
            auto d = parse_unsigned<u32>(tokens[i + 2]);
            auto l = parse_unsigned<u32>(tokens[i + 3]);
            if (w && d && l) {
                data.wdl = WDL{*w, *d, *l};
            }
            i += 3;
        } else if (token == "pv" && i + 1 < tokens.size()) {
            for (u64 j = i + 1; j < tokens.size(); ++j) {
                data.pv.emplace_back(tokens[j]);
            }
            break;
        }
    }
    return data;
}

inline std::optional<InfoData> parse_info_string(std::string_view line) {
    InfoData data;
    data.raw_string = line;

    constexpr std::string_view NNUE_PREFIX = "NNUE evaluation";
    constexpr u64 NNUE_VALUE_OFFSET = 18;
    
    if (auto pos = line.find(NNUE_PREFIX); pos != std::string_view::npos) {
        std::string_view eval_part = line.substr(pos + NNUE_VALUE_OFFSET);
        std::vector<std::string_view> tokens = split(eval_part, ' ');
        if (tokens.size() >= 1) {
            if (auto val = parse_float(tokens[0])) {
                data.static_eval = Score{Score::Type::Centipawns,
                                         static_cast<i32>(*val * 100.0)};
            }
        }
    }
    return data;
}

inline std::vector<std::string_view> split(std::string_view str, char delimiter) {
    std::vector<std::string_view> result;
    u64 start = 0;
    u64 end = str.find(delimiter);
    while (end != std::string_view::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
}

inline std::optional<Score> parse_score(std::string_view type, std::string_view value) {
    Score s;
    if (type == "cp") {
        s.type = Score::Type::Centipawns;
        s.value = parse_signed<i32>(value).value_or(0);
        return s;
    }
    if (type == "mate") {
        s.type = Score::Type::Mate;
        s.value = parse_signed<i32>(value).value_or(0);
        return s;
    }
    return std::nullopt;
}

inline std::optional<f64> parse_float(std::string_view sv) {
    f64 value{};
    auto result = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return (result.ec == std::errc()) ? std::optional(value) : std::nullopt;
}

} // namespace detail

} // namespace vgce::uci
