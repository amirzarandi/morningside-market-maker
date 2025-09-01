#pragma once

#include "types.hpp"
#include <string>

struct Underlying {
    std::string name;
    UnderlyingId underlying_id;
    Price valuation;
    Probability down_move_probability;
    Price down_move_step;
    Price noise_std_dev;
    Probability up_move_probability;
    Price up_move_step;

    Underlying(std::string name, UnderlyingId id, Price val, 
                Probability down_prob, Price down_step, Price noise,
                Probability up_prob, Price up_step);
    
    Underlying(const Underlying&) = default;
    Underlying(Underlying&&) noexcept = default;
    
    Underlying& operator=(const Underlying&) = default;
    Underlying& operator=(Underlying&&) noexcept = default;

    ~Underlying() = default;
    
    bool operator==(const Underlying& other) const noexcept;
    
    UnderlyingPtr advance_step() const;

private:
    void validate_parameters() const;
};