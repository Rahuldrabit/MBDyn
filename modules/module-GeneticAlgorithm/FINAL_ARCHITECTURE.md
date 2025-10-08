# Genetic Algorithm Module - Final Architecture

## Overview
The GA system has been successfully restructured with clear separation of responsibilities:

**Architecture:**
- **libga.so** - Population initialization ONLY
- **libcons.so** - Fitness evaluation ONLY  
- **module-genetic-algorithm.cc** - ALL GA operations (selection, crossover, mutation)

## Files Structure

### Core Implementation (Active)

1. **module-genetic-algorithm.cc** (19KB - MERGED & COMPLETE)
   - Main UDE implementation
   - Parses .mbd input file
   - Loads libga.so and libcons.so
   - Implements ALL GA logic:
     - Selection (tournament, roulette, rank)
     - Crossover (one-point, two-point, uniform, blend)
     - Mutation (gaussian, uniform)
     - Elite preservation
     - Evolution pipeline
   - Manages population state
   - Exposes outputs via private data

2. **libga_init.cc** (3.7KB - COMPILED)
   - Minimal population initialization library
   - Functions:
     - `ga_init_population()` - Initialize with random individuals
     - `ga_get_individual()` - Read individual genes
     - `ga_set_individual()` - Write individual genes
     - `ga_get_population_size()` - Query population size
     - `ga_get_chromosome_length()` - Query gene count
     - `ga_cleanup()` - Free resources
   - Built as: `libga.so`

3. **libcons.cc** (1.6KB - COMPILED)
   - Fitness evaluation library
   - Functions:
     - `evaluate_fitness(genes, n_genes, inputs, n_inputs)` - Calculate fitness
     - `evaluate_constraint()` - Evaluate constraints
   - Example: Rastrigin function
   - Built as: `libcons.so`

4. **Operator Implementations** (Compiled into module)
   - `crossover/crossover.h` & `crossover.cc`
   - `mutation/mutation.h` & `mutation.cc`
   - `selection-operator/selection-operator.h` & `selection-operator.cc`

### Testing & Validation

5. **test_architecture.cc**
   - Validates the modular architecture
   - Tests libga.so + libcons.so + module logic
   - Successfully runs and confirms separation of concerns

### Legacy Files (Reference Only)

6. **libga_unified.cc** (25KB)
   - Previous unified approach
   - Kept for reference
   - NOT used in new architecture

7. **module-genetic-algorithm-standalone.cc** (DELETED)
   - Was a duplicate implementation
   - Successfully merged into module-genetic-algorithm.cc
   - No longer needed

## Build Status

✅ **libga.so** - Built successfully (120KB)
✅ **libcons.so** - Built successfully  
✅ **test_architecture** - Runs successfully
⏳ **module-genetic-algorithm.so** - Needs MBDyn build system

## Input File Format

The module parses `.mbd` files with this syntax:

```
user defined: <label>, genetic algorithm,
    inputs number, <n_inputs>,
    [input drives...],
    genetic algorithm, "<libga_path>",
    constraint function, "<libcons_path>",
    population, <population_size>,
    generation, <n_generations>,
    crossover, "<crossover_type>",      # two_point, one_point, uniform, blend
    mutation, "<mutation_type>",         # gaussian, uniform
    selection, "<selection_type>",       # tournament, roulette, rank
    mutation rate, <rate>,
    crossover rate, <rate>,
    elite ratio, <ratio>,
    output number, <n_outputs>,
    <output_labels...>;
```

## How It Works

### 1. Initialization (Constructor)
```
User .mbd file → MBDyn Parser
                      ↓
            module-genetic-algorithm.cc
                      ↓
            Load libga.so (dlopen)
                      ↓
            ga_init_population() ← Initialize random population
                      ↓
            Copy to module storage
                      ↓
            Create operator instances
```

### 2. Evolution (Each Timestep)
```
AssRes() called by MBDyn
        ↓
Sample input drives
        ↓
evolveGeneration() [MODULE METHOD]
        ↓
    ┌─────────────────────────┐
    │  evaluatePopulation()   │ ← Uses libcons.so
    │  For each individual:   │
    │    fitness = evaluate_  │
    │      fitness()          │
    └─────────────────────────┘
        ↓
    ┌─────────────────────────┐
    │  selectElite()          │ ← MODULE logic
    │  Keep best individuals  │
    └─────────────────────────┘
        ↓
    ┌─────────────────────────┐
    │  performCrossover()     │ ← MODULE operators
    │  selectParent()         │
    │  two_point_crossover→   │
    │  one_point_crossover→   │
    │  etc.                   │
    └─────────────────────────┘
        ↓
    ┌─────────────────────────┐
    │  applyMutation()        │ ← MODULE operators
    │  gaussianMutation()     │
    │  uniformMutation()      │
    └─────────────────────────┘
        ↓
Update best_individual
Update m_outputs
```

### 3. Output (Expose Results)
```
Other MBDyn elements request private data
        ↓
dGetPrivData() called
        ↓
Return best_fitness or output[i]
```

## Responsibilities

### libga.so
- ✅ Initialize population with random values
- ✅ Store population in memory
- ✅ Provide get/set access to individuals
- ❌ NO selection
- ❌ NO crossover
- ❌ NO mutation
- ❌ NO evolution

### libcons.so
- ✅ Evaluate fitness for given genes
- ✅ Evaluate constraints
- ❌ NO population management
- ❌ NO operator logic
- ❌ NO evolution

### module-genetic-algorithm.cc
- ✅ Load both libraries
- ✅ Parse .mbd configuration
- ✅ Initialize population (via libga.so)
- ✅ Implement ALL selection operators
- ✅ Implement ALL crossover operators
- ✅ Implement ALL mutation operators
- ✅ Orchestrate evolution pipeline
- ✅ Evaluate fitness (via libcons.so)
- ✅ Track best individual
- ✅ Expose outputs to MBDyn

## Validation Results

From test_architecture.cc:
```
=== Testing Modular GA Architecture ===
✓ libga.so: Successfully initialized population (10 individuals, 5 genes)
✓ libcons.so: Successfully evaluated fitness (best: -64.69)
✓ Module logic: Successfully performed mutation (new fitness: -60.01)

Architecture validated!
  - Libraries provide minimal services
  - Module controls ALL GA operations
```

## Next Steps

To build the complete module:

1. Build MBDyn to generate `mbconfig.h`
2. Use MBDyn's build system:
   ```bash
   cd /home/rahul/MBDyn_GSoC/MBDyn
   ./bootstrap.sh
   ./configure
   make -C modules/module-GeneticAlgorithm
   ```

3. Or use the standalone build approach (without full MBDyn headers)

## Summary

✅ **Architecture implemented exactly as requested:**
- libga.so: population initialization only
- libcons.so: fitness evaluation only
- module: ALL GA operations (selection, crossover, mutation, evolution)

✅ **All code merged into single module file**
✅ **Redundant files removed**
✅ **Architecture validated with test program**
✅ **Ready for MBDyn integration**
