# Genetic Algorithm Optimization Module for MBDyn

This module provides genetic algorithm optimization capabilities for MBDyn simulations. It allows you to optimize parameters of structural elements using external shared libraries.

## Input Format

The genetic algorithm optimization element uses the following syntax:

```
user defined: <label>, genetic algorithm optimization,
    
    inputs number, N,
        element, <id>, joint, string, "<component>", direct,
        element, <id>, joint, string, "<component>", direct,
        ...

    genetic algorithm, "<libga_path>",
    constraint function, "<libcons_path>", 
    population, <population_size>,
    generations, <num_generations>,

    outputs number, N,
        node, <node_id>,
        node, <node_id>,
        ...;
```

### Parameters

- **inputs number**: Number of optimization parameters
- **element entries**: Define which element parameters to optimize
  - `element`: Element ID to optimize
  - `joint`: Type of element (currently supports joint)
  - `string`: Component to optimize (e.g., "M[2]" for moment component)
  - `direct`: Direct parameter access mode
- **genetic algorithm**: Path to GA shared library (.so file)
- **constraint function**: Path to constraint shared library (.so file)  
- **population**: Population size for genetic algorithm
- **generations**: Number of generations to run
- **outputs**: Node IDs to monitor during optimization

## Shared Library Interface

### Genetic Algorithm Library (libga.so)

Must implement these functions:

```c
// Generate initial population
void generate_population(double* population, int popSize, int genomeLen);

// Calculate fitness for a genome
double fitness(const double* genome, int genomeLen);

// Perform crossover between parents
void crossover(const double* parent1, const double* parent2, 
               double* child1, double* child2, 
               int genomeLen, double crossoverRate);

// Mutate a genome
void mutate(double* genome, int genomeLen, double mutationRate, double mutationStrength);

// Select parent using tournament selection
int tournament_selection(const double* population, const double* fitness_values,
                        int popSize, int genomeLen, int tournamentSize);
```

### Constraint Library (libcons.so)

Must implement:

```c
// Check if genome satisfies constraints
int constraint_ok(const double* genome, int genomeLen);

// Calculate constraint penalty
double constraint_penalty(const double* genome, int genomeLen);

// Check bounds constraints
int check_bounds(const double* genome, const double* lower_bounds, 
                 const double* upper_bounds, int genomeLen);

// Get detailed constraint information
int constraint_details(const double* genome, int genomeLen,
                      int* violations, int* num_violations);
```

## Building

1. Build the shared libraries:
```bash
make all
```

2. Install libraries (optional):
```bash
make install
```

3. Build the MBDyn module (follow standard MBDyn module build process)

## Example Usage

See `example_input.mbd` for a complete example that optimizes 5 joint parameters using genetic algorithm optimization.

## Library Development

The provided example implementations show:

- **libga_impl.c**: Basic GA operations with single-point crossover, Gaussian mutation, and tournament selection
- **libcons_impl.c**: Example constraints including bounds checking, sum constraints, and nonlinear constraints

You can customize these implementations or create your own libraries following the interface specifications.

## Integration with HFElem

This module can be used with the HFElem module to optimize harmonic forcing parameters. The amplitude parameter `m_dAmplitude` from HFElem can be included as one of the optimization inputs.
