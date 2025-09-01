#include "option.hpp"
#include <stdexcept>
#include <algorithm>

Option::Option(OptionId id, OptionType type, Steps steps, Strike s, 
        UnderlyingId u_id, std::string u_name)
    : option_id(id), option_type(type), steps_until_expiry(steps),
        strike(s), underlying_id(u_id), underlying_name(std::move(u_name)) {
    
    if (steps_until_expiry < 0) {
        throw std::invalid_argument("Steps until expiry must be non-negative");
    }
}

OptionPtr Option::from_underlying(const Underlying& underlying, OptionId option_id,
                                OptionType option_type, Steps steps_until_expiry,
                                Strike strike) {
    return std::make_shared<Option>(option_id, option_type, steps_until_expiry, 
                                    strike, underlying.underlying_id, underlying.name);
}

OptionPtr Option::advance_step() const {
    if (steps_until_expiry == 0) {
        return std::make_shared<Option>(*this);
    }
    
    return std::make_shared<Option>(option_id, option_type, steps_until_expiry - 1,
                                    strike, underlying_id, underlying_name);
}

bool Option::contract_matches(const Option& other) const noexcept {
    return option_type == other.option_type &&
            steps_until_expiry == other.steps_until_expiry &&
            strike == other.strike &&
            underlying_id == other.underlying_id &&
            underlying_name == other.underlying_name;
}

Price Option::expiry_valuation(Price underlying_valuation) const noexcept {
    if (option_type == OptionType::CALL) {
        return std::max(0.0, underlying_valuation - strike);
    }
    return std::max(0.0, static_cast<double>(strike) - underlying_valuation);
}

std::string Option::to_string() const {
    std::string result;
    result.reserve(64);
    
    result += std::to_string(option_id);
    result += " (";
    result += std::to_string(steps_until_expiry);
    result += "s ";
    result += underlying_name;
    result += " ";
    result += std::to_string(strike);
    result += to_string_view(option_type);
    result += ")";
    
    return result;
}

bool Option::operator==(const Option& other) const noexcept {
    return option_id == other.option_id &&
            option_type == other.option_type &&
            steps_until_expiry == other.steps_until_expiry &&
            strike == other.strike &&
            underlying_id == other.underlying_id &&
            underlying_name == other.underlying_name;
}