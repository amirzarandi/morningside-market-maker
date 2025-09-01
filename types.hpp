#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <tuple>
#include <string_view>

struct Underlying;
struct Option;

using OptionId = int;
using UnderlyingId = int;
using Price = double;
using Quantity = double;
using Steps = int;
using Strike = int;
using Probability = double;

using Greeks = std::tuple<Price, Price, Price>;
using BidAsk = std::tuple<Price, Price>;
using OptionQuantityMap = std::unordered_map<OptionId, int>;
using UnderlyingQuantityMap = std::unordered_map<UnderlyingId, Quantity>;
using PriceCache = std::unordered_map<std::string, Greeks>;
using DeltaMap = std::unordered_map<UnderlyingId, Price>;
using TradeCallback = std::function<void(UnderlyingId, Quantity)>;

using UnderlyingPtr = std::shared_ptr<const Underlying>;
using OptionPtr = std::shared_ptr<const Option>;
using UnderlyingVector = std::vector<UnderlyingPtr>;
using OptionVector = std::vector<OptionPtr>;

enum class OptionType {
    CALL,
    PUT
};

constexpr std::string_view to_string_view(OptionType type) noexcept {
    return type == OptionType::CALL ? "C" : "P";
}

std::string to_string(OptionType type);

constexpr int MAX_POSITIONS = 50;