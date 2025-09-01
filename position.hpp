#pragma once

#include "types.hpp"
#include <cmath>

class Position {
public:
    OptionQuantityMap option_quantity_by_option_id;
    UnderlyingQuantityMap underlying_quantity_by_underlying_id;
    
    Position() {
        option_quantity_by_option_id.reserve(16);
        underlying_quantity_by_underlying_id.reserve(8);
    }

    Position(const Position&) = default;
    Position(Position&&) noexcept = default;

    Position& operator=(const Position&) = default;
    Position& operator=(Position&&) noexcept = default;
    
    void add_option_quantity(OptionId option_id, int quantity) {
        option_quantity_by_option_id[option_id] += quantity;
    }
    
    void add_underlying_quantity(UnderlyingId underlying_id, Quantity quantity) {
        quantity = std::round(quantity * 100.0) / 100.0;
        underlying_quantity_by_underlying_id[underlying_id] += quantity;
    }
};