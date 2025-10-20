# GA Module Runtime Integration Summary

## Changes Made

### 1. Fixed API Mismatch
**Problem**: Module called `ga_init_population()` but `libga_init.cc` exports `ga_ctx_create()`.

**Solution**: Updated all function pointers to match libga_init.cc API:
- `ga_ctx_create(popSize, chromLen, lower, upper, seed)`
- `ga_ctx_destroy(ctx)`
- `ga_randomize_population(ctx, sigma_fraction)`
- `ga_register_env_ptrs(ctx, inputs, nInputs, outputs, nOutputs, desired)`
- `ga_evaluate_population(ctx)`
- `ga_best_index(ctx)`
- `ga_get_best_individual(ctx, out_genes)`
- `ga_get_top_n(ctx, N, out_genes)`

### 2. Added Missing Keywords
Added parser support for:
- `population_upper <value>` — sets upper bound for all genes
- `population_lower <value>` — sets lower bound for all genes
- `return_best_population <N>` — returns top N individuals to outputs

### 3. Fixed Library Path Parsing
**Problem**: User wrote `population , "./libga.so"` which conflicts with `population, 50`.

**Corrected syntax**:
```
genetic algorithm, "./libga.so",    # library path
population, 50,                      # population size
```

### 4. Wired Runtime Flow
Module now:
1. Creates GA context via `ga_ctx_create()`
2. Randomizes initial population via `ga_randomize_population()`
3. Registers environment pointers via `ga_register_env_ptrs()`
4. Each timestep in `AssRes()`:
   - Samples input drives
   - Runs module-driven evolution
   - Calls `ga_evaluate_population()` to evaluate via libga
   - Calls `ga_get_top_n(N, out_genes)` to get best N individuals
   - Maps genes to outputs (flat array: ind0_genes, ind1_genes, ...)

### 5. Output Mapping
If `return_best_population = 3` and `chromosome_length = 5`:
- Need 3 × 5 = 15 output labels
- Layout: `[ind0_g0, ind0_g1, ..., ind0_g4, ind1_g0, ..., ind2_g4]`

## Corrected .mbd Syntax

```text
user defined: 1000, genetic algorithm optimization,
    inputs number, 5,
        element, 101, joint, string, "M[2]", direct,
        element, 102, joint, string, "M[2]", direct,
        element, 103, joint, string, "M[2]", direct,
        element, 104, joint, string, "M[2]", direct,
        element, 105, joint, string, "M[2]", direct,
    
    genetic algorithm, "./libga.so",      # ← FIXED: separate keyword
    constraint function, "./libcons.so",
    
    population, 50,                       # ← FIXED: integer value
    generation, 10,
    
    crossover, "two_point",
    mutation, "uniform",
    selection, "tournament",
    mutation rate, 0.4,
    crossover rate, 0.3,
    
    population_lower, 0.0,                # ← NEW
    population_upper, 1.0,                # ← NEW
    return_best_population, 3,            # ← NEW: return top 3
    
    output number, 15,                    # ← FIXED: 3 individuals × 5 genes = 15
        121, 122, 123, 124, 125,          # Individual 0 (best)
        126, 127, 128, 129, 130,          # Individual 1
        131, 132, 133, 134, 135;          # Individual 2
```

## Build & Test

To compile the updated module and libga:
```bash
cd /home/rahul/MBDyn_GSoC/MBDyn/modules/module-GeneticAlgorithm

# Build libga.so from libga_init.cc
g++ -std=c++17 -fPIC -shared libga_init.cc -o libga.so

# Build libcons.so
g++ -std=c++17 -fPIC -shared libcons.cc -o libcons.so

# Test architecture (optional)
g++ -std=c++17 test_architecture.cc -ldl -o test_architecture
./test_architecture
```

## Next Steps

1. Build module in full MBDyn tree (requires configured headers)
2. Run with `test_ga_runtime.mbd` to verify runtime flow
3. Check output labels 121-135 contain best individual genes
4. Optionally add `chrom_length` keyword if you want explicit control (currently inferred as 5 or from variables block)

