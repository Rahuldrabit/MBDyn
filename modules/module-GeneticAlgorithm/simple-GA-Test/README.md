# Simple Genetic Algorithm Test Suite

This directory contains a comprehensive implementation and testing framework for a simple genetic algorithm (GA) designed to solve real-valued optimization problems.

## Overview

The Simple GA Test Suite demonstrates the application of genetic algorithms to benchmark optimization functions including Rastrigin, Ackley, and Schwefel functions. The implementation uses real-valued chromosomes and standard GA operators for selection, crossover, and mutation.

## Project Structure

```
simple-GA-Test/
├── README.md                  # This documentation file
├── Makefile                   # Build configuration
├── fitness-function.h         # Fitness function declarations
├── fitness-fuction.cc         # Fitness function implementations
└── ../simple-ga-pipline.cc    # Main GA implementation
```

## Features

### Genetic Algorithm Components

- **Representation**: Real-valued chromosomes with configurable length
- **Selection**: Tournament selection with elitism
- **Crossover**: One-point crossover with configurable rate
- **Mutation**: Gaussian mutation with bounds checking
- **Replacement**: Generational replacement with elitism

### Benchmark Functions

1. **Rastrigin Function**
   - Highly multimodal with many local optima
   - Global minimum: f(0,...,0) = 0
   - Search domain: [-5.12, 5.12]^n
   - Challenge: Many local minima

2. **Ackley Function**
   - One global minimum with many local minima
   - Global minimum: f(0,...,0) = 0
   - Search domain: [-32.768, 32.768]^n
   - Challenge: Exponential components create complex landscape

3. **Schwefel Function**
   - Deceptive function with global optimum far from local optima
   - Global minimum: f(420.9687,...,420.9687) ≈ 0
   - Search domain: [-500, 500]^n
   - Challenge: Global optimum is far from second-best local optima

## Configuration Parameters

The GA can be configured through the `GAConfig` structure:

```cpp
struct GAConfig {
    int populationSize = 50;        // Size of the population
    int generations = 50;           // Number of generations
    int chromosomeLength = 10;      // Number of variables
    double mutationRate = 0.01;     // Mutation probability
    double crossoverRate = 0.8;     // Crossover probability
    double eliteRatio = 0.1;        // Fraction of elites to preserve
    
    // Function-specific bounds (auto-set based on function type)
    double lowerBound = -5.12;
    double upperBound = 5.12;
    
    FunctionType function = RASTRIGIN;  // Target function
    bool verbose = true;                // Print progress
    string outputFile = "ga_results.txt"; // Results file
};
```

## Building and Running

### Prerequisites

- C++17 compatible compiler (g++, clang++)
- Make utility

### Build Instructions

```bash
# Navigate to the test directory
cd /home/rahul/MBDyn-GA/MBDyn/modules/module-GeneticAlgorithm/simple-GA-Test

# Build the project
make

# Run the tests
make run

# Clean build files
make clean
```

### Manual Compilation

```bash
g++ -std=c++17 -Wall -Wextra -O3 -I.. \
    ../simple-ga-pipline.cc \
    fitness-fuction.cc \
    ../selection-operator/selection-operator.cc \
    ../crossover/crossover.cc \
    ../mutation/mutation.cc \
    ../crossover/logger_init.cc \
    ../mutation/logger_init.cc \
    -o simple_ga_test
```

## Usage Examples

### Basic Usage

```cpp
#include "simple-ga-pipline.cc"

int main() {
    GAConfig config;
    config.populationSize = 100;
    config.generations = 100;
    config.function = GAConfig::RASTRIGIN;
    
    SimpleGA ga(config);
    ga.initializePopulation();
    ga.evolve();
    
    return 0;
}
```

### Running All Benchmark Tests

The main program automatically runs tests on all three benchmark functions:

```bash
./simple_ga_test
```

This will:
1. Test the GA on Rastrigin function
2. Test the GA on Ackley function  
3. Test the GA on Schwefel function
4. Generate result files for each test
5. Display performance statistics

## Output Files

The GA generates CSV files with fitness evolution data:

- `ga_rastrigin_results.txt` - Rastrigin function results
- `ga_ackley_results.txt` - Ackley function results
- `ga_schwefel_results.txt` - Schwefel function results

### File Format

```csv
Generation,BestFitness,AvgFitness
0,45.234,23.456
1,52.123,28.789
...
```

## Performance Analysis

### Expected Results

For well-tuned parameters, the GA should:

1. **Rastrigin**: Converge to fitness values > 900 (function values < 10)
2. **Ackley**: Converge to fitness values > 950 (function values < 5)
3. **Schwefel**: More challenging, may require longer runs

### Convergence Patterns

- **Fast initial improvement**: Population diversity drives rapid fitness gains
- **Plateau phase**: Exploration vs exploitation balance
- **Fine-tuning**: Local search behavior as population converges

## Customization

### Adding New Fitness Functions

1. Implement the function in `fitness-fuction.cc`:
```cpp
double myFunction(const std::vector<double>& x) {
    // Your implementation
}

double myFunctionFitness(const std::vector<double>& x) {
    return 1000.0 / (1.0 + myFunction(x));
}
```

2. Add function type to `GAConfig` enum
3. Update the evaluation switch statement

### Modifying GA Parameters

```cpp
GAConfig config;
config.populationSize = 200;     // Larger population
config.generations = 1000;       // Longer evolution
config.mutationRate = 0.05;      // Higher mutation
config.crossoverRate = 0.9;      // Higher crossover
config.eliteRatio = 0.05;        // Fewer elites
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure C++17 support: `g++ --version`
   - Check include paths: `-I..`

2. **Poor Convergence**
   - Increase population size
   - Adjust mutation rate
   - Try different parameter combinations

3. **Build Failures**
   - Clean and rebuild: `make clean && make`
   - Check dependencies

### Debug Mode

Enable verbose output for detailed information:

```cpp
config.verbose = true;
```

## Dependencies

The test suite depends on the following modules:

- **Selection Operators**: Tournament, roulette wheel, rank selection
- **Crossover Operators**: One-point, two-point, uniform crossover
- **Mutation Operators**: Gaussian, uniform, creep mutation
- **Logging System**: Comprehensive logging for debugging

## Performance Metrics

The implementation tracks:

- **Best fitness per generation**
- **Average fitness per generation**
- **Execution time**
- **Convergence statistics**

## Contributing

To extend the test suite:

1. Add new benchmark functions to `fitness-fuction.cc`
2. Implement additional GA variants in `simple-ga-pipline.cc`
3. Add parameter sensitivity analysis
4. Include multi-objective versions

## References

1. Goldberg, D.E. (1989). "Genetic Algorithms in Search, Optimization and Machine Learning"
2. Eiben, A.E. & Smith, J.E. (2015). "Introduction to Evolutionary Computing"
3. Back, T. (1996). "Evolutionary Algorithms in Theory and Practice"

## License

This implementation is part of the MBDyn project. See the main project license for details.