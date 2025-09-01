#include "market_maker.hpp"
#include <algorithm>
#include <cmath>

MarketMaker::MarketMaker(UnderlyingVector underlying_initial_state,
            OptionVector option_initial_state)
    : BaseMarketMaker(std::move(underlying_initial_state), std::move(option_initial_state)) {
    
    price_cache.reserve(1024);
    last_underlying_prices.reserve(8);
    target_deltas.reserve(8);
    hedge_pos.reserve(8);
    last_hedge.reserve(8);
}

std::string MarketMaker::cache_key_string(OptionId option_id, Price price) const {
    std::string key;
    key.reserve(32);
    key += std::to_string(option_id);
    key += "_";
    key += std::to_string(price);
    return key;
}

Price MarketMaker::portfolio_value() {
    Price total = pnl;
    
    for (const auto& option_ptr : active_option_state) {
        const auto& option = *option_ptr;
        auto it = position.option_quantity_by_option_id.find(option.option_id);
        if (it != position.option_quantity_by_option_id.end() && it->second != 0) {
            total += it->second * price_option(option);
        }
    }
    
    for (const auto& u_ptr : underlying_state) {
        const auto& u = *u_ptr;
        auto it = position.underlying_quantity_by_underlying_id.find(u.underlying_id);
        if (it != position.underlying_quantity_by_underlying_id.end() && it->second != 0) {
            total += it->second * u.valuation;
        }
    }
    
    return total;
}

bool MarketMaker::check_risk_limit() {
    Price curr_value = portfolio_value();
    if (curr_value < max_loss) {
        safe_mode = true;
        return true;
    }
    
    if (safe_mode && curr_value > max_loss * 0.5) {
        safe_mode = false;
    }
    
    return safe_mode;
}

Price MarketMaker::price_option_from_scratch(const Option& option, const Underlying& underlying) {
    int n = option.steps_until_expiry;
    
    std::vector<Price> tree;
    tree.reserve(n + 1);
    tree.resize(n + 1);
    
    for (int i = 0; i <= n; ++i) {
        int up_moves = i;
        int down_moves = n - i;
        
        Price terminal = underlying.valuation + up_moves * underlying.up_move_step 
                       - down_moves * underlying.down_move_step;
        terminal = std::max(terminal, 0.0);
        
        if (option.option_type == OptionType::CALL) {
            tree[i] = std::max(terminal - option.strike, 0.0);
        } else {
            tree[i] = std::max(static_cast<double>(option.strike) - terminal, 0.0);
        }
    }
    
    for (int step = n; step > 0; --step) {
        for (int i = 0; i < step; ++i) {
            tree[i] = underlying.up_move_probability * tree[i + 1] 
                    + underlying.down_move_probability * tree[i];
        }
    }
    
    return tree[0];
}

std::unique_ptr<Underlying> MarketMaker::pump_it_up(const Underlying& underlying, Price bump) const {
    return std::make_unique<Underlying>(
        underlying.name, underlying.underlying_id,
        std::max(0.0, underlying.valuation + bump),
        underlying.down_move_probability, underlying.down_move_step,
        underlying.noise_std_dev, underlying.up_move_probability,
        underlying.up_move_step);
}

Price MarketMaker::calculate_delta(const Option& option, const Underlying& underlying, Price base_price) {
    Price bump_size = std::min(1.0, underlying.up_move_step * 0.1);
    
    auto bumped_underlying = pump_it_up(underlying, bump_size);
    Price bumped = price_option_from_scratch(option, *bumped_underlying);
    
    return (bumped - base_price) / bump_size;
}

Price MarketMaker::calculate_gamma(const Option& option, const Underlying& underlying) {
    Price bump_size = std::min(1.0, underlying.up_move_step * 0.1);
    
    Price center = price_option_from_scratch(option, underlying);
    
    auto up_u = pump_it_up(underlying, bump_size);
    Price up_price = price_option_from_scratch(option, *up_u);
    
    auto down_u = pump_it_up(underlying, -bump_size);
    Price down_price = price_option_from_scratch(option, *down_u);
    
    return (up_price - 2 * center + down_price) / (bump_size * bump_size);
}

Greeks MarketMaker::get_greeks(const Option& option, const Underlying& underlying) {
    Price curr_price = underlying.valuation;
    std::string key = cache_key_string(option.option_id, curr_price);
    
    auto it = price_cache.find(key);
    if (it != price_cache.end()) {
        return it->second;
    }
    
    Price price = price_option_from_scratch(option, underlying);
    Price delta = calculate_delta(option, underlying, price);
    Price gamma = calculate_gamma(option, underlying);
    
    Greeks greeks = std::make_tuple(price, delta, gamma);
    price_cache.emplace(std::move(key), greeks);
    return greeks;
}

const Underlying* MarketMaker::find_underlying(UnderlyingId u_id) const {
    auto it = std::find_if(underlying_state.begin(), underlying_state.end(),
                            [u_id](const UnderlyingPtr& u) { return u->underlying_id == u_id; });
    return (it != underlying_state.end()) ? it->get() : nullptr;
}

Price MarketMaker::portfolio_delta(UnderlyingId u_id) {
    const Underlying* underlying = find_underlying(u_id);
    if (!underlying) return 0.0;
    
    Price total = 0.0;
    
    for (const auto& opt_ptr : active_option_state) {
        const auto& opt = *opt_ptr;
        if (opt.underlying_id == u_id) {
            auto pos_it = position.option_quantity_by_option_id.find(opt.option_id);
            if (pos_it != position.option_quantity_by_option_id.end() && pos_it->second != 0) {
                auto [price, delta, gamma] = get_greeks(opt, *underlying);
                total += pos_it->second * delta;
            }
        }
    }
    
    auto hedge_it = hedge_pos.find(u_id);
    if (hedge_it != hedge_pos.end()) {
        total -= hedge_it->second;
    }
    
    return total;
}

void MarketMaker::delta_hedge_post_trade(const Option& option, int q) {
    const Underlying* underlying = find_underlying(option.underlying_id);
    if (!underlying) return;
    
    auto [price, delta, gamma] = get_greeks(option, *underlying);
    
    Price delta_exposure = q * delta;
    target_deltas[underlying->underlying_id] += delta_exposure;
    
    Price net_delta = portfolio_delta(underlying->underlying_id);
    
    if (std::abs(net_delta) > HEDGE_TH) {
        exec_delta_hedge(underlying->underlying_id, net_delta);
    }
}

void MarketMaker::exec_delta_hedge(UnderlyingId u_id, Price target) {
    Price current_hedge = hedge_pos[u_id];
    Price hedge_trade = target - current_hedge;
    
    if (std::abs(hedge_trade) < MIN_HEDGE) {
        return;
    }
    
    try {
        if (hedge_trade > 0) {
            sell_underlying(u_id, std::abs(hedge_trade));
        } else {
            buy_underlying(u_id, std::abs(hedge_trade));
        }
        
        hedge_pos[u_id] = current_hedge + hedge_trade;
    } catch (const std::exception&) {
    }
}

void MarketMaker::rehedge(const UnderlyingVector& new_u_state) {
    for (const auto& u_ptr : new_u_state) {
        const auto& u = *u_ptr;
        UnderlyingId u_id = u.underlying_id;
        Price cprice = u.valuation;
        
        auto last_price_it = last_underlying_prices.find(u_id);
        Price lprice = (last_price_it != last_underlying_prices.end()) ? last_price_it->second : cprice;
        
        Price diff = cprice - lprice;
        
        if (std::abs(diff) < GAMMA_SCALP_TH) {
            continue;
        }
        
        Price net_delta = portfolio_delta(u_id);
        
        if (std::abs(net_delta) > HEDGE_TH) {
            Price hedge_trade = net_delta;
            
            try {
                Price current_hedge = hedge_pos[u_id];
                
                if (hedge_trade > 0) {
                    buy_underlying(u_id, std::abs(hedge_trade));
                } else {
                    sell_underlying(u_id, std::abs(hedge_trade));
                }
                
                hedge_pos[u_id] = current_hedge + hedge_trade;
                
            } catch (const std::exception&) {
            }
        }
        
        last_hedge[u_id] = cprice;
    }
}

BidAsk MarketMaker::make_market(const Option& option) {
    if (check_risk_limit()) {
        return std::make_tuple(0.01, 99999999.0);
    }
    
    Price fair = price_option(option);
    
    auto curr_pos_it = position.option_quantity_by_option_id.find(option.option_id);
    int curr_pos = (curr_pos_it != position.option_quantity_by_option_id.end()) ? curr_pos_it->second : 0;
    
    Price base_spread = std::max(0.01, fair * 0.02);
    
    const Underlying* underlying = find_underlying(option.underlying_id);
    if (!underlying) {
        return std::make_tuple(0.01, 99999999.0);
    }
    
    auto [price, delta, gamma] = get_greeks(option, *underlying);
    
    Price gamma_adj = std::min(0.5, std::abs(gamma) * underlying->valuation * 0.1);
    
    Price time_adj = 1.0;
    if (option.steps_until_expiry <= 2) {
        time_adj = 2.0;
    } else if (option.steps_until_expiry <= 5) {
        time_adj = 1.3;
    }
    
    Price spread = base_spread * time_adj * (1 + gamma_adj);
    
    Price bid = std::max(0.0, fair - spread / 2);
    Price ask = fair + spread / 2;
    
    if (curr_pos > MAX_POSITIONS) {
        bid = 0.01;
    } else if (curr_pos < -MAX_POSITIONS) {
        ask = ask * 10;
    }
    
    return std::make_tuple(bid, ask);
}

Price MarketMaker::price_option(const Option& option) {
    const Underlying* underlying = find_underlying(option.underlying_id);
    if (!underlying) {
        return 0.0;
    }
    
    if (option.steps_until_expiry == 0) {
        return option.expiry_valuation(underlying->valuation);
    }
    
    Price curr_price = underlying->valuation;
    std::string cache_key = cache_key_string(option.option_id, curr_price);
    
    auto it = price_cache.find(cache_key);
    if (it != price_cache.end()) {
        return std::get<0>(it->second);
    }
    
    auto last_price_it = last_underlying_prices.find(underlying->underlying_id);
    if (last_price_it != last_underlying_prices.end() && last_price_it->second != curr_price) {
        Price price_diff = std::abs(curr_price - last_price_it->second);
        
        if (price_diff < underlying->up_move_step * 0.1) {
            std::string old_cache_key = cache_key_string(option.option_id, last_price_it->second);
            auto old_it = price_cache.find(old_cache_key);
            if (old_it != price_cache.end()) {
                auto [old_price, delta, gamma] = old_it->second;
                Price dS = curr_price - last_price_it->second;
                Price price = old_price + delta * dS + 0.5 * gamma * dS * dS;
                
                Price new_delta = calculate_delta(option, *underlying, price);
                Price new_gamma = calculate_gamma(option, *underlying);
                
                price_cache.emplace(std::move(cache_key), std::make_tuple(price, new_delta, new_gamma));
                return price;
            }
        }
    }
    
    Price price = price_option_from_scratch(option, *underlying);
    Price delta = calculate_delta(option, *underlying, price);
    Price gamma = calculate_gamma(option, *underlying);
    price_cache.emplace(std::move(cache_key), std::make_tuple(price, delta, gamma));
    
    last_underlying_prices[underlying->underlying_id] = curr_price;
    
    return price;
}

void MarketMaker::on_bid_hit(const Option& option, Price bid_price) {
    BaseMarketMaker::on_bid_hit(option, bid_price);
    pnl += bid_price;
    delta_hedge_post_trade(option, 1);
}

void MarketMaker::on_offer_hit(const Option& option, Price offer_price) {
    BaseMarketMaker::on_offer_hit(option, offer_price);
    pnl -= offer_price;
    delta_hedge_post_trade(option, -1);
}

void MarketMaker::on_step_advance(UnderlyingVector new_underlying_state,
                    OptionVector new_option_state) {
    BaseMarketMaker::on_step_advance(std::move(new_underlying_state), std::move(new_option_state));

    std::unordered_set<OptionId> active_options;
    active_options.reserve(active_option_state.size());
    for (const auto& opt_ptr : active_option_state) {
        active_options.insert(opt_ptr->option_id);
    }
    
    auto it = price_cache.begin();
    while (it != price_cache.end()) {
        const std::string& key = it->first;
        size_t underscore_pos = key.find('_');
        if (underscore_pos != std::string::npos) {
            OptionId opt_id = std::stoi(key.substr(0, underscore_pos));
            if (active_options.find(opt_id) == active_options.end()) {
                it = price_cache.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (price_cache.size() > 100000) {
        auto keys_to_remove = price_cache.begin();
        for (int i = 0; i < 50000 && keys_to_remove != price_cache.end(); ++i) {
            auto to_remove = keys_to_remove++;
            price_cache.erase(to_remove);
        }
    }
    
    rehedge(underlying_state);
    
    for (const auto& u_ptr : underlying_state) {
        last_underlying_prices[u_ptr->underlying_id] = u_ptr->valuation;
    }
}