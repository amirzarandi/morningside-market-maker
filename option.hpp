#pragma once

#include "types.hpp"
#include "underlying.hpp"
#include <string>

struct Option {
    OptionId option_id;
    OptionType option_type;
    Steps steps_until_expiry;
    Strike strike;
    UnderlyingId underlying_id;
    std::string underlying_name;

    Option(OptionId id, OptionType type, Steps steps, Strike s, 
            UnderlyingId u_id, std::string u_name);

    Option(const Option&) = default;
    Option(Option&&) noexcept = default;
    
    Option& operator=(const Option&) = default;
    Option& operator=(Option&&) noexcept = default;
    
    ~Option() = default;
    
    static OptionPtr from_underlying(const Underlying& underlying, OptionId option_id,
                                    OptionType option_type, Steps steps_until_expiry,
                                    Strike strike);
    
    OptionPtr advance_step() const;
    
    bool contract_matches(const Option& other) const noexcept;
    
    Price expiry_valuation(Price underlying_valuation) const noexcept;
    
    std::string to_string() const;
    
    bool operator==(const Option& other) const noexcept;
};