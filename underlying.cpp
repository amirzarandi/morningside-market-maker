#include "underlying.hpp"
#include <stdexcept>
#include <random>
#include <algorithm>
#include <cmath>

std::string to_string(OptionType type) {
    return std::string(to_string_view(type));
}

Underlying::Underlying(std::string name, UnderlyingId id, Price val, 
            Probability down_prob, Price down_step, Price noise,
            Probability up_prob, Price up_step)
    : name(std::move(name)), underlying_id(id), valuation(val),
        down_move_probability(down_prob), down_move_step(down_step),
        noise_std_dev(noise), up_move_probability(up_prob), up_move_step(up_step) {
    
    validate_parameters();
}

bool Underlying::operator==(const Underlying& other) const noexcept {
    return underlying_id == other.underlying_id;
}

UnderlyingPtr Underlying::advance_step() const {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<> uniform(0.0, 1.0);
    static thread_local std::normal_distribution<> normal(0.0, 1.0);
    
    Price new_valuation;
    if (uniform(gen) < up_move_probability) {
        new_valuation = valuation + up_move_step;
    } else {
        new_valuation = valuation - down_move_step;
    }
    
    new_valuation += normal(gen) * noise_std_dev;
    new_valuation = std::max(new_valuation, 0.0);
    new_valuation = std::round(new_valuation * 100.0) / 100.0;
    
    return std::make_shared<Underlying>(
        name, underlying_id, new_valuation, down_move_probability,
        down_move_step, noise_std_dev, up_move_probability, up_move_step);
}

void Underlying::validate_parameters() const {
    if (down_move_step <= 0 || up_move_step <= 0) {
        throw std::invalid_argument("Down/up move steps must both be positive");
    }
    
    if (down_move_probability <= 0 || up_move_probability <= 0) {
        throw std::invalid_argument("Down/up move probabilities must both be positive");
    }
    
    if (std::abs(down_move_probability + up_move_probability - 1.0) > 1e-9) {
        throw std::invalid_argument("Down and up move probabilities must sum to 1");
    }
    
    if (std::abs((down_move_probability * down_move_step) - (up_move_probability * up_move_step)) > 1e-5) {
        throw std::invalid_argument("Underlying has drift");
    }
}