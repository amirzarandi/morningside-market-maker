#include "market_maker.hpp"
#include <iostream>
#include <iomanip>

void print_separator(std::string_view title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void print_underlying_state(const UnderlyingVector& underlyings) {
    std::cout << "Underlying Assets:\n";
    for (const auto& u_ptr : underlyings) {
        const auto& u = *u_ptr;
        std::cout << "  " << u.name << " (ID: " << u.underlying_id << "): $" 
                    << std::fixed << std::setprecision(2) << u.valuation << "\n";
    }
}

void print_option_state(const OptionVector& options) {
    std::cout << "\nActive Options:\n";
    for (const auto& opt_ptr : options) {
        const auto& opt = *opt_ptr;
        std::cout << "  " << opt.to_string() << "\n";
    }
}

void print_bid_ask(const Option& option, const BidAsk& quote) {
    const auto& [bid, ask] = quote;
    std::cout << "  " << option.to_string() << "\n";
    std::cout << "    Bid: $" << std::fixed << std::setprecision(4) << bid 
                << ", Ask: $" << ask << ", Spread: $" << (ask - bid) << "\n";
}

void print_position_summary(MarketMaker& mm) {
    std::cout << "\nPosition Summary:\n";
    std::cout << "Option Positions:\n";
    for (const auto& [option_id, quantity] : mm.position.option_quantity_by_option_id) {
        if (quantity != 0) {
            std::cout << "  Option " << option_id << ": " << quantity << " contracts\n";
        }
    }
    
    std::cout << "Underlying Positions:\n";
    for (const auto& [underlying_id, quantity] : mm.position.underlying_quantity_by_underlying_id) {
        if (std::abs(quantity) > 1e-6) {
            std::cout << "  Underlying " << underlying_id << ": " 
                        << std::fixed << std::setprecision(4) << quantity << " shares\n";
        }
    }
}

UnderlyingVector create_underlyings() {
    UnderlyingVector underlyings;
    underlyings.reserve(2);
    
    underlyings.emplace_back(std::make_shared<Underlying>("CULIONS", 1, 150.0, 0.5, 2.0, 0.1, 0.5, 2.0));
    underlyings.emplace_back(std::make_shared<Underlying>("SEAS", 2, 200.0, 0.5, 3.0, 0.2, 0.5, 3.0));
    
    return underlyings;
}

OptionVector create_options(const UnderlyingVector& underlyings) {
    OptionVector options;
    options.reserve(4);
    
    options.emplace_back(Option::from_underlying(*underlyings[0], 1001, OptionType::CALL, 5, 152));
    options.emplace_back(Option::from_underlying(*underlyings[0], 1002, OptionType::PUT, 5, 148));
    options.emplace_back(Option::from_underlying(*underlyings[1], 1003, OptionType::CALL, 3, 205));
    options.emplace_back(Option::from_underlying(*underlyings[1], 1004, OptionType::PUT, 3, 195));
    
    return options;
}

UnderlyingVector advance_underlyings(const UnderlyingVector& current_underlyings) {
    UnderlyingVector new_underlyings;
    new_underlyings.reserve(current_underlyings.size());
    
    for (const auto& underlying_ptr : current_underlyings) {
        new_underlyings.emplace_back(underlying_ptr->advance_step());
    }
    
    return new_underlyings;
}

OptionVector advance_options(const OptionVector& current_options) {
    OptionVector new_options;
    new_options.reserve(current_options.size());
    
    for (const auto& option_ptr : current_options) {
        new_options.emplace_back(option_ptr->advance_step());
    }
    
    return new_options;
}

void print_market_movement(const UnderlyingVector& old_underlyings, 
                            const UnderlyingVector& new_underlyings) {
    std::cout << "Market moves\n";
    for (size_t i = 0; i < new_underlyings.size(); ++i) {
        const auto& old_u = *old_underlyings[i];
        const auto& new_u = *new_underlyings[i];
        
        std::cout << "  " << new_u.name << ": $" << old_u.valuation 
                    << " -> $" << std::fixed << std::setprecision(2) << new_u.valuation;
        
        double change = new_u.valuation - old_u.valuation;
        std::cout << " (" << (change >= 0 ? "+" : "") << change << ")\n";
    }
}

int main() {
    try {
        print_separator("MORNINGSIDE MARKET MAKER SIMULATION");

        auto underlyings = create_underlyings();
        auto options = create_options(underlyings);
        
        print_underlying_state(underlyings);
        print_option_state(options);

        MarketMaker mm{UnderlyingVector(underlyings), OptionVector(options)};

        mm.register_trade_underlying_callback([](UnderlyingId id, Quantity qty) {
            std::cout << "  Trading underlying " << id << ": " 
                        << std::fixed << std::setprecision(4) << qty << " shares\n";
        });
        
        print_separator("INITIAL MARKET MAKING");
        
        std::cout << "Market Maker Quotes:\n";
        for (const auto& option_ptr : options) {
            const auto& option = *option_ptr;
            BidAsk quote = mm.make_market(option);
            print_bid_ask(option, quote);
        }
        
        print_separator("SIMULATING TRADES");

        const auto& option0 = *options[0];
        const auto& option3 = *options[3];
        
        auto call_quote = mm.make_market(option0);
        std::cout << "Client hits bid on " << option0.to_string() 
                    << " at $" << std::get<0>(call_quote) << "\n";
        mm.on_bid_hit(option0, std::get<0>(call_quote));

        auto put_quote = mm.make_market(option3);
        std::cout << "Client lifts offer on " << option3.to_string() 
                    << " at $" << std::get<1>(put_quote) << "\n";
        mm.on_offer_hit(option3, std::get<1>(put_quote));
        
        print_position_summary(mm);
        
        print_separator("MARKET MOVEMENT - STEP 1");
        
        auto new_underlyings = advance_underlyings(underlyings);
        auto new_options = advance_options(options);
        
        print_market_movement(underlyings, new_underlyings);

        mm.on_step_advance(std::move(new_underlyings), std::move(new_options));
        
        print_separator("NEW QUOTES AFTER MOVEMENT");

        std::cout << "Updated Market Maker Quotes:\n";
        for (const auto& option_ptr : mm.active_option_state) {
            const auto& option = *option_ptr;
            BidAsk quote = mm.make_market(option);
            print_bid_ask(option, quote);
        }
        
        print_position_summary(mm);
        
        print_separator("MARKET MOVEMENT - STEP 2");

        auto prev_underlyings = UnderlyingVector(mm.underlying_state);
        auto final_underlyings = advance_underlyings(mm.underlying_state);
        auto final_options = advance_options(mm.active_option_state);
        
        print_market_movement(prev_underlyings, final_underlyings);
        
        mm.on_step_advance(std::move(final_underlyings), std::move(final_options));
        
        print_separator("FINAL STATE");
        
        print_underlying_state(mm.underlying_state);
        print_option_state(mm.active_option_state);
        
        std::cout << "\nFinal Market Maker Quotes:\n";
        for (const auto& option_ptr : mm.active_option_state) {
            const auto& option = *option_ptr;
            BidAsk quote = mm.make_market(option);
            print_bid_ask(option, quote);
        }
        
        print_position_summary(mm);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}