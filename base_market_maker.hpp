#pragma once

#include "types.hpp"
#include "position.hpp"
#include "option.hpp"
#include "underlying.hpp"
#include <stdexcept>

class BaseMarketMaker {
public:
    UnderlyingVector underlying_state;
    OptionVector active_option_state;
    Position position;
    TradeCallback trade_underlying_callback;
    
    BaseMarketMaker(UnderlyingVector underlying_initial_state,
                    OptionVector option_initial_state)
        :   underlying_state(std::move(underlying_initial_state)),
            active_option_state(std::move(option_initial_state)) {
        
        this->underlying_state.reserve(8);
        this->active_option_state.reserve(32);
    }
    
    virtual ~BaseMarketMaker() = default;

    BaseMarketMaker(const BaseMarketMaker&) = delete;
    BaseMarketMaker& operator=(const BaseMarketMaker&) = delete;
    BaseMarketMaker(BaseMarketMaker&&) noexcept = default;
    BaseMarketMaker& operator=(BaseMarketMaker&&) noexcept = default;
    
    void buy_underlying(UnderlyingId underlying_id, Quantity quantity) {
        if (quantity <= 0) {
            throw std::invalid_argument("Trade quantity must be positive");
        }
        
        trade_underlying_callback(underlying_id, quantity);
        position.add_underlying_quantity(underlying_id, quantity);
    }
    
    virtual BidAsk make_market(const Option& option) = 0;
    
    virtual void on_bid_hit(const Option& option, Price /* bid_price */) {
        position.add_option_quantity(option.option_id, 1);
    }
    
    virtual void on_offer_hit(const Option& option, Price /* offer_price */) {
        position.add_option_quantity(option.option_id, -1);
    }
    
    virtual void on_step_advance(UnderlyingVector new_underlying_state,
                                OptionVector new_option_state) {
        underlying_state = std::move(new_underlying_state);
        active_option_state = std::move(new_option_state);
    }
    
    void register_trade_underlying_callback(TradeCallback callback) {
        trade_underlying_callback = std::move(callback);
    }
    
    virtual Price price_option(const Option& option) = 0;
    
    void sell_underlying(UnderlyingId underlying_id, Quantity quantity) {
        if (quantity <= 0) {
            throw std::invalid_argument("Trade quantity must be positive");
        }
        
        trade_underlying_callback(underlying_id, -quantity);
        position.add_underlying_quantity(underlying_id, -quantity);
    }
};