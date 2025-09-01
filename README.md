# Morningside Market Maker

We implement an options market maker with binomial tree pricing, Greeks calculation, delta hedging, and gamma scalping strategies.

## Usage

```bash
make
./market_maker_sim

make clean
```
We demonstrate how to construct underlying assets and European-style options with various strikes and expirations in `main.cpp`. The Morningside Market Maker then generates bid/ask quotes on each advance of the underlying. 

The Polymorphic extensibility of our framework allows pluggable pricing and hedging strategies.

## Theory

### Binomial Tree Option Pricing

We implement a discrete-time binomial tree model for option valuation, because it's easy :). This means we model the underlying asset's price evolution as a multiplicative random walk over discrete time steps. This model uses the following key inputs:

- **Current underlying price (S₀)**
- **Up move step (u)**: the absolute price increase in an up move
- **Down move step (d)**: the absolute price decrease in a down move  
- **Up move probability (p)**: risk-neutral probability of upward movement
- **Down move probability (1-p)**: risk-neutral probability of downward movement
- **Time steps (n)**: number of discrete time steps until expiration
- **Strike price (K)**

At each time step, the underlying price can move to one of two possible states:

$$S_{up} = S_0 + u$$
$$S_{down} = S_0 - d$$

After n steps, the terminal price at node i is calculated as:

$$S_T(i) = S_0 + i \cdot u - (n-i) \cdot d$$

where i represents the number of up moves (0 ≤ i ≤ n).

This model satisfies the no-arbitrage condition: expected price change is zero under the risk-neutral measure.

$$p \cdot u - (1-p) \cdot d = 0$$

Option values are calculated using backward induction from expiration to present:

Terminal payoffs at expiration are
   - Call option: $\max(S_T - K, 0)$
   - Put option: $\max(K - S_T, 0)$

Hence for earlier nodes we have the recursive relation
   $$V_{t,i} = p \cdot V_{t+1,i+1} + (1-p) \cdot V_{t+1,i}$$

and the root node value is the present value $V_{0,0}$

### Greeks

TLDR: We calculate first and second-order derivatives of option prices with respect to underlying parameters.

#### Delta (Δ)
Measures the rate of change of option price with respect to underlying price:

$$\Delta = \frac{\partial V}{\partial S} \approx \frac{V(S + \epsilon) - V(S)}{\epsilon}$$

Where ε is a small bump size (we use 1% of the up move step).

#### Gamma (Γ) 
Measures the rate of change of delta with respect to underlying price:

$$\Gamma = \frac{\partial^2 V}{\partial S^2} \approx \frac{V(S + \epsilon) - 2V(S) + V(S - \epsilon)}{\epsilon^2}$$

### Delta Hedging

Delta hedging aims to create a market-neutral position by offsetting option delta exposure with underlying positions.

For each option position, the required hedge is:
$$\text{Hedge Quantity} = -\text{Option Quantity} \times \Delta$$

As the underlying price moves and time passes, delta changes, requiring position adjustments:

1. We calculate portfolio delta $\Delta_{portfolio} = \sum_i q_i \times \Delta_i$
2. Then we determine hedge adjustment as $\text{Hedge Trade} = -\Delta_{portfolio}$  
3. If adjustment exceeds minimum threshold execute trade

### Gamma Scalping

Gamma scalping exploits the convexity of option positions to generate profits from underlying price movements. When the underlying moves, a gamma-positive position benefits from:

1. **Profitable rehedging**: Buying low after down moves, selling high after up moves
2. **Time decay capture**: Earning theta while maintaining delta neutrality

### Performance Optimizations

#### Price Caching
We cache calculated Greeks to avoid redundant computations:
```cpp
std::string cache_key = std::to_string(option_id) + "_" + std::to_string(price);
```

#### Taylor Series Approximation
For small price movements, option prices are approximated using Taylor expansion:

$$V(S + \Delta S) \approx V(S) + \Delta \cdot \Delta S + \frac{1}{2}\Gamma \cdot (\Delta S)^2$$

This approximation is used when:
$$|\Delta S| < 0.1 \times \text{up\_move\_step}$$

### Random Walk Model

The underlying asset follows a discrete random walk with:
- **Drift-free condition**: $p \cdot u = (1-p) \cdot d$ (ensures no systematic bias)
- **Noise component**: Gaussian noise with standard deviation σ
- **Price floors**: Prices cannot go negative
- **Rounding**: Prices rounded to nearest cent

The complete price evolution is:
$$S_{t+1} = \max(0, \text{round}(S_t + \text{move} + \mathcal{N}(0, \sigma), 2))$$

where move is either +u (with probability p) or -d (with probability 1-p).

### Risk Management

#### Position Limits
- **Maximum position**: ±50 contracts per option
- **Position-based pricing**: Wider spreads for large positions

#### Loss Limits
- **Maximum loss threshold**: -$50,000
- **Safe mode**: Triggered when loss limit exceeded
- **Recovery threshold**: 50% recovery from maximum loss

#### Market Making Adjustments
Spreads are adjusted based on:
1. **Base spread**: 2% of fair value
2. **Gamma adjustment**: Up to 50% wider for high-gamma options
3. **Time decay factor**: 2x wider for options with ≤2 steps to expiry
4. **Position skew**: Asymmetric quotes based on current position



