# GA Module Runtime Configuration Guide

This guide explains how your input file connects to the GA module and operator implementations.

## Input File Structure

```
user defined: 1000, genetic algorithm optimization,
    inputs number, 5,
        element, 101, joint, string, "M[2]", direct,
        element, 102, joint, string, "M[2]", direct,
        element, 103, joint, string, "M[2]", direct,
        element, 104, joint, string, "M[2]", direct,
        element, 105, joint, string, "M[2]", direct,
    genetic algorithm: libga.so,
    constraint function: libcons.so,
    population, 50,
    generation, 10,
    mutation, uniform,
    crossover, two_point,
    selection, tournament,
    mutation rate, 0.4,
    crossover rate, 0.3,
    output number, 3,
        121,
        123,
        126;
```

## How It Works

### 1. Initialization Phase (Constructor)

**Parser reads your input:**
- `inputs number, 5` → Creates 5 `DriveCaller` instances
- Each `element, 101, joint, ...` line → Parses a drive specification that will sample joint M[2] component
- `genetic algorithm: libga.so` → dlopens your unified GA library
- `constraint function: libcons.so` → dlopens fitness/constraint library (optional)
- `population, 50` → Sets GA population size
- `generation, 10` → Sets number of generations to evolve
- `crossover, two_point` → Stores operator name
- `mutation, uniform` → Stores operator name  
- `selection, tournament` → Stores operator name
- `mutation rate, 0.4` → Stores mutation probability
- `crossover rate, 0.3` → Stores crossover probability
- `output number, 3` with labels 121/123/126 → Registers 3 output drives

**Library loading:**
```cpp
gaLibHandle = dlopen("libga.so", RTLD_LAZY);
gaInit = (GAInitFunc)dlsym(gaLibHandle, "ga_init");
gaSetOperators = (GASetOperatorsFunc)dlsym(gaLibHandle, "ga_set_operators");
gaSetMutationRate = (GASetMutationRateFunc)dlsym(gaLibHandle, "ga_set_mutation_rate");
gaSetCrossoverRate = (GASetCrossoverRateFunc)dlsym(gaLibHandle, "ga_set_crossover_rate");
// ... other symbols
```

**GA context creation:**
```cpp
gaCtx = gaInit(population, generations);  // Creates GAContext with 50 individuals, 10 generations

// Configure operators from your .mbd keywords
gaSetOperators(gaCtx, "two_point", "uniform", "tournament");
gaSetMutationRate(gaCtx, 0.4);
gaSetCrossoverRate(gaCtx, 0.3);
```

### 2. Runtime Phase (Each Timestep in AssRes)

**Input sampling:**
```cpp
// MBDyn solver has already called DriveHandler::SetTime() and LinkToSolution()
for (size_t i = 0; i < m_inputs.size(); ++i) {
    m_inputVals[i] = m_inputs[i].pDC->dGet();  // Reads joint M[2] value
}
```

**GA evolution:**
```cpp
// Pass sampled inputs to GA (conditioning variables)
if (gaSetInputs) {
    gaSetInputs(gaCtx, m_inputVals.data(), m_inputVals.size());
}

// Run GA with configured operators
gaRun(gaCtx);  // Inside libga_unified.cc:
                // - Uses TwoPointCrossoverWrapper (from crossover.cc)
                // - Uses UniformMutationWrapper (from mutation.cc)  
                // - Uses TournamentSelectionWrapper (from selection-operator.cc)
                // - Applies mutation_rate=0.4, crossover_rate=0.3

// Get results
m_fitness = gaGetBest(gaCtx);
gaGetOutputs(gaCtx, m_outputs.data(), m_outputs.size());
```

**Output exposure:**
- `m_outputs[0]` is available at drive label 121
- `m_outputs[1]` is available at drive label 123
- `m_outputs[2]` is available at drive label 126
- `m_fitness` is available via `dGetPrivData(1)` (if you register a fitness drive label)

### 3. Operator Implementation Flow

**libga_unified.cc OperatorFactory:**
```cpp
// When gaSetOperators(ctx, "two_point", "uniform", "tournament") is called:

// 1. Factory creates TwoPointCrossoverWrapper
crossover_op = factory.createCrossover("two_point");
// → TwoPointCrossoverWrapper contains TwoPointCrossover from crossover.cc

// 2. Factory creates UniformMutationWrapper  
mutation_op = factory.createMutation("uniform");
// → UniformMutationWrapper contains MutationOperators::uniformMutation from mutation.cc

// 3. Factory creates TournamentSelectionWrapper
selection_op = factory.createSelection("tournament");
// → TournamentSelectionWrapper wraps TournamentSelection() from selection-operator.cc
```

**During evolveGeneration():**
```cpp
// Crossover phase
auto offspring = crossover_op->crossover(parent1.genes, parent2.genes);
// → Calls TwoPointCrossoverWrapper::crossover()
//   → Calls TwoPointCrossover::crossover() from crossover.cc
//   → Uses your two-point crossover algorithm

// Mutation phase  
mutation_op->mutate(individual, mutation_rate, lower_bounds, upper_bounds);
// → Calls UniformMutationWrapper::mutate()
//   → Calls MutationOperators::uniformMutation() from mutation.cc
//   → Uses your uniform mutation algorithm with rate=0.4

// Selection phase
Individual selected = selection_op->select(population, rng);
// → Calls TournamentSelectionWrapper::select()
//   → Calls TournamentSelection() from selection-operator.cc
//   → Uses your tournament selection algorithm
```

## Library Responsibilities

### libga.so (libga_unified.cc)
- **Exports C API:** ga_init, ga_run, ga_set_operators, ga_set_mutation_rate, ga_set_crossover_rate, ga_get_best, ga_get_outputs
- **GAContext:** Manages population, fitness values, best solution
- **OperatorFactory:** Maps operator names to wrapper instances
- **Wrappers:** Adapt your operator implementations to unified interfaces
- **Evolution loop:** Coordinates selection, crossover, mutation, elitism

### crossover.cc
- **TwoPointCrossover:** Your two-point crossover implementation
- **OnePointCrossover:** Your one-point crossover implementation
- **UniformCrossover:** Your uniform crossover implementation
- **BlendCrossover:** Your blend/BLX-α crossover implementation

### mutation.cc
- **MutationOperators::uniformMutation:** Random gene replacement within bounds
- **MutationOperators::gaussianMutation:** Gaussian perturbation of genes

### selection-operator.cc
- **TournamentSelection():** Select k random individuals, return fittest
- **RouletteWheelSelection():** Fitness-proportionate selection
- **RankSelection():** Rank-based selection with linear bias

### libcons.so (optional fitness library)
- If provided, exports custom fitness function
- Called per individual during fitness evaluation
- Can access MBDyn state via callbacks (advanced usage)

## Available Operators

### Crossover operators (use with `crossover, <name>`)
- `two_point` - Two-point crossover
- `one_point` - Single-point crossover
- `uniform` - Uniform crossover
- `blend` - BLX-α blend crossover

### Mutation operators (use with `mutation, <name>`)
- `uniform` - Uniform random mutation
- `gaussian` - Gaussian perturbation

### Selection operators (use with `selection, <name>`)
- `tournament` - Tournament selection
- `roulette` - Roulette wheel selection
- `rank` - Rank-based selection

## Build Instructions

```bash
cd /home/rahul/MBDyn_GSoC/MBDyn/modules/module-GeneticAlgorithm

# Build unified GA library with your operators
./build_unified_ga.sh

# Build stub constraint library (or replace with your fitness implementation)
g++ -fPIC -shared -o libcons.so libcons_stub.cc

# Verify symbols
nm -D libga.so | grep ga_set
nm -D libcons.so | grep fitness  # if applicable
```

## Testing

```bash
# Quick standalone test
./test_unified_ga

# Full MBDyn integration test (requires MBDyn built with module support)
mbdyn -f test_runtime_config.mbd
```

## Troubleshooting

### "GA symbols missing from library"
- Rebuild libga.so with `./build_unified_ga.sh`
- Check symbols: `nm -D libga.so | grep ga_`

### "Failed to set operators"
- Check operator name spelling (case-sensitive)
- Available operators listed by `ga_list_operators()`
- Factory will warn and use defaults if name not found

### "Constraint function library not found"
- libcons.so is optional
- Build stub: `g++ -fPIC -shared -o libcons.so libcons_stub.cc`
- Or omit the `constraint function` line from .mbd

### Input drives return zero
- Ensure joints/elements 101-105 exist and are active
- Check drive specification syntax
- Verify DriveHandler is linked before AssRes

## Advanced: Custom Fitness via libcons.so

To implement custom fitness evaluation:

1. Create `my_fitness.cc`:
```cpp
extern "C" {
    double evaluate_fitness(const double* genes, int n_genes, 
                           const double* inputs, int n_inputs) {
        // Your fitness logic here
        double fitness = 0.0;
        for (int i = 0; i < n_genes; i++) {
            fitness += genes[i] * genes[i];  // Example: minimize sum of squares
        }
        return -fitness;  // Negative because GA maximizes
    }
}
```

2. Build: `g++ -fPIC -shared -o libcons.so my_fitness.cc`

3. The GA library can dlsym and call `evaluate_fitness` during evolution

## Summary

Your input file keywords directly map to:
- `inputs number` → Drive sampling via `DriveCaller::dGet()`
- `genetic algorithm` → dlopen libga.so (contains unified GA + your operators)
- `population/generation` → ga_init parameters
- `crossover/mutation/selection` → ga_set_operators with your operator names
- `mutation rate/crossover rate` → ga_set_mutation_rate/ga_set_crossover_rate
- `output number` → Output vector size returned by ga_get_outputs

The unified library automatically uses your implementations from `crossover.cc`, `mutation.cc`, and `selection-operator.cc` based on the operator names you specify in the .mbd file!
