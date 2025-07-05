#ifndef RANK_SELECTION_HPP
#define RANK_SELECTION_HPP

#include <vector>
#include <algorithm>
#include <random>
#include <numeric>
#include <stdexcept>

// -------------------------------------------------------------
//  Minimal individual structure (extend as needed)
// -------------------------------------------------------------
struct Individual {
    Population
    double fitness;
    // add chromosome / genes here
};

// -------------------------------------------------------------
//  Rank‑based selection operator (linear or exponential)
// -------------------------------------------------------------
/**
 * @tparam RNG  Uniform random bit generator (e.g. std::mt19937_64)
 * @param population         Vector of Individuals
 * @param numSelections      How many parents to pick
 * @param rng                RNG instance (passed by reference)
 * @param exponential        true  -> exponential (geometric) ranking
 *                           false -> linear ranking (default)
 * @param expBase            Base (>1) used for exponential ranking
 * @param withReplacement    true  -> sampling with replacement (default)
 *                           false -> without replacement
 *
 * @return Vector of selected indices into the original population
 */
template <typename RNG>
std::vector<std::size_t>
RankSelection(const std::vector<Individual>& population,
              std::size_t                     numSelections,
              RNG&                            rng,
              bool                            exponential     = false,
              double                          expBase         = 1.07,
              bool                            withReplacement = true)
{
    const std::size_t n = population.size();
    if (n == 0) throw std::invalid_argument("Population is empty");
    if (numSelections == 0) return {};

    // ---------------------------------------------------------
    // 1. Sort indices by fitness (descending)
    // ---------------------------------------------------------
    std::vector<std::size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&](std::size_t a, std::size_t b) {
                  return population[a].fitness > population[b].fitness; // higher is better
              });

    // ---------------------------------------------------------
    // 2. Build rank‑based weights
    //    Best individual gets weight n (linear) or expBase^n (exponential)
    // ---------------------------------------------------------
    std::vector<double> weights(n);
    for (std::size_t rank = 0; rank < n; ++rank) {
        std::size_t r = n - rank; // rank 1 .. n  (best .. worst)
        weights[rank] = exponential ? std::pow(expBase, static_cast<double>(r))
                                    : static_cast<double>(r);
    }

    // ---------------------------------------------------------
    // 3. Draw parents according to the discrete distribution
    // ---------------------------------------------------------
    std::vector<std::size_t> selected;
    selected.reserve(numSelections);

    if (withReplacement) {
        std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
        for (std::size_t i = 0; i < numSelections; ++i) {
            selected.push_back(indices[dist(rng)]);
        }
    } else {
        // Without replacement: re‑generate distribution each time
        std::vector<std::size_t> poolIdx   = indices;
        std::vector<double>      poolWghts = weights;

        for (std::size_t k = 0; k < numSelections && !poolIdx.empty(); ++k) {
            std::discrete_distribution<std::size_t> dist(poolWghts.begin(), poolWghts.end());
            std::size_t pos = dist(rng);

            selected.push_back(poolIdx[pos]);
            poolIdx.erase(poolIdx.begin() + pos);
            poolWghts.erase(poolWghts.begin() + pos);
        }
    }

    return selected;
}

// -------------------------------------------------------------
//  Example usage
// -------------------------------------------------------------
/*
#include "rank_selection.hpp"
#include <iostream>

int main() {
    std::vector<Individual> pop = { {10.0}, {4.5}, {7.1}, {15.8}, {2.3} };
    std::mt19937_64 rng(std::random_device{}());

    auto parents = RankSelection(pop, 3, rng, false); // linear ranking

    std::cout << "Selected indices:";
    for (auto idx : parents) std::cout << ' ' << idx;
    std::cout << '\n';
}
*/

#endif // RANK_SELECTION_HPP
