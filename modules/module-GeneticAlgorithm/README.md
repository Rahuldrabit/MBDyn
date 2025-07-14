# MBDyn Genetic Algorithm Optimization Module

## Overview

This module implements a Genetic Algorithm (GA) optimization framework for MBDyn, allowing users to perform parameter optimization within multibody dynamics simulations. The module enables automatic tuning of model parameters to achieve desired system behavior according to user-defined fitness functions.

## Features

- Integration with existing MBDyn elements
- User-defined fitness functions
- Constraint handling capabilities
- Parallel evaluation of fitness functions
- Configurable GA parameters (population size, mutation rate, crossover rate, etc.)
- **NEW**: Comprehensive test suite with benchmark optimization functions

## Test Suite

The module includes a simple GA pipeline for testing with three benchmark optimization functions:

### Benchmark Functions

1. **Rastrigin Function**: Highly multimodal with many local optima
   - Global minimum: f(0,...,0) = 0
   - Search domain: [-5.12, 5.12]^n

2. **Ackley Function**: Has one global minimum and many local minima  
   - Global minimum: f(0,...,0) = 0
   - Search domain: [-32.768, 32.768]^n

3. **Schwefel Function**: Deceptive function with global optimum far from local optima
   - Global minimum: f(420.9687,...,420.9687) ≈ 0
   - Search domain: [-500, 500]^n

### Running the Test Suite

```bash
cd simple-GA-Test
make run
```

This will:
- Build the test executable
- Run GA optimization on all three benchmark functions
- Generate results files for each function
- Display progress and final results

### Test Configuration

The default test configuration uses:
- Population size: 50
- Generations: 50
- Chromosome length: 10 variables
- Mutation rate: 1%
- Crossover rate: 80%
- Elite ratio: 10%

## Installation

To build the Genetic Algorithm module:

1. Ensure MBDyn is properly installed on your system
2. Navigate to the module directory:
   ```
   cd /path/to/MBDyn/modules/module-GeneticAlgorithm
   ```
3. Build the module:
   ```
   ./configure
   make
   make install
   ```

## Usage

The Genetic Algorithm module can be used in MBDyn input files as follows:

```
user defined: <label>, genetic algorithm,
    inputs: <number_of_inputs>,
    elements: <list_of_element_labels>,
    element types: <list_of_element_types>,
    data names: <list_of_data_names>,
    outputs: <number_of_outputs>,
    output drives: <list_of_drive_labels>,
    fitness function: "<expression>",
    constraints function: "<expression>",
    # Selection methods
    selection_method: "<method>",  # "tournament", "roulette_wheel", "rank_based", "sus", "elitism"
    tournament_size: <size>,
    # GA specific parameters
    population size: <size>,
    generations: <number>,
    mutation rate: <rate>,
    crossover rate: <rate>,
    elite count: <count>,
    # Optional parameters
    initial population: "<filename>",
    output results: "<filename>"
;
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `inputs` | Number of input parameters to be optimized |
| `elements` | Labels of elements whose parameters will be modified |
| `element types` | Types of elements to be modified |
| `data names` | Names of specific data fields to be modified |
| `outputs` | Number of output values to be evaluated |
| `output drives` | Drive labels providing simulation output values |
| `fitness function` | Mathematical expression defining the objective function |
| `constraints function` | Expression defining constraints on parameters |
| `selection_method` | Selection algorithm: "tournament", "roulette_wheel", "rank_based", "sus", "elitism" |
| `tournament_size` | Number of individuals in tournament selection (default: 3) |
| `population size` | Number of individuals in each generation |
| `generations` | Maximum number of generations to run |
| `mutation rate` | Probability of mutation during reproduction |
| `crossover rate` | Probability of crossover during reproduction |
| `elite count` | Number of top individuals to preserve in each generation |

## Example

Here's a simple example of optimizing spring stiffness and damping parameters:

```
# Define the optimization problem
user defined: 100, genetic algorithm,
    inputs: 2,
    elements: 1 2,
    element types: "deformable joint" "deformable joint",
    data names: "stiffness" "damping",
    outputs: 1,
    output drives: 3,
    fitness function: "abs(y_target - y_actual)",
    constraints function: "k > 1000 && k < 10000 && c > 10 && c < 100",
    selection_method: "tournament",
    tournament_size: 3,
    population size: 50,
    generations: 100,
    mutation rate: 0.1,
    crossover rate: 0.8,
    elite count: 5
;
```

## Development

To extend the Genetic Algorithm module:

1. Modify `module-GeneticAlgorithm.h` to add new GA features or parameters
2. Implement the corresponding functionality in `module-GeneticAlgorithm.cc`
3. Rebuild the module

## Testing and Validation

The test suite provides a comprehensive way to validate GA performance:

```bash
# Run all benchmark tests
cd simple-GA-Test
make run

# Clean build files
make clean

# Build only
make all
```

Results are saved as CSV files that can be analyzed with plotting tools to visualize convergence behavior.

## License

This module is distributed under the same license as MBDyn.

## Contact

For issues, suggestions, or contributions, please contact the MBDyn development team.
