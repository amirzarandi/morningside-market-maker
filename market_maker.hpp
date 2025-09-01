#pragma once

#include "base_market_maker.hpp"
#include <unordered_set>
#include <memory>

class MarketMaker : public BaseMarketMaker {
private:
    PriceCache price_cache;
    std::unordered_map<UnderlyingId, Price> last_underlying_prices;
    DeltaMap target_deltas;
    DeltaMap hedge_pos;
    std::unordered_map<UnderlyingId, Price> last_hedge;
    
    static constexpr Price MIN_HEDGE = 0.05;
    static constexpr Price HEDGE_TH = 0.03;
    static constexpr Price GAMMA_SCALP_TH = 0.005;
    
    Price pnl = 0.0;
    static constexpr Price max_loss = -50000.0;
    bool safe_mode = false;
    
    std::string cache_key_string(OptionId option_id, Price price) const;
    Price portfolio_value();
    bool check_risk_limit();
    Price price_option_from_scratch(const Option& option, const Underlying& underlying);
    std::unique_ptr<Underlying> pump_it_up(const Underlying& underlying, Price bump) const;
    Price calculate_delta(const Option& option, const Underlying& underlying, Price base_price);
    Price calculate_gamma(const Option& option, const Underlying& underlying);
    Greeks get_greeks(const Option& option, const Underlying& underlying);
    const Underlying* find_underlying(UnderlyingId u_id) const;
    Price portfolio_delta(UnderlyingId u_id);
    void delta_hedge_post_trade(const Option& option, int q);
    void exec_delta_hedge(UnderlyingId u_id, Price target);
    void rehedge(const UnderlyingVector& new_u_state);
    
public:
    MarketMaker(UnderlyingVector underlying_initial_state,
                OptionVector option_initial_state);
    
    BidAsk make_market(const Option& option) override;
    Price price_option(const Option& option) override;
    void on_bid_hit(const Option& option, Price bid_price) override;
    void on_offer_hit(const Option& option, Price offer_price) override;
    void on_step_advance(UnderlyingVector new_underlying_state,
                        OptionVector new_option_state) override;
};