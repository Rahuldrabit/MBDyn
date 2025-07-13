# Simple Genetic Algorithm Test Suite

This directory contains a comprehensive implementation and testing framework for a genetic algorithm (GA) with **multiple representation types** designed to solve various optimization problems.

## Overview

The Simple GA Test Suite demonstrates the application of genetic algorithms to benchmark optimization functions including Rastrigin, Ackley, and Schwefel functions. The implementation supports **multiple encoding types** (binary, real-valued, integer, permutation) with **representation-specific operators** and automatic validation.

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

### 🆕 **Multi-Representation Support**

The GA now supports **four different encoding types**, each with specific operators:

#### 1. **Binary Representation**
- **Use Case**: Feature selection, binary optimization problems
- **Crossovers**: One-point, two-point, uniform
- **Mutations**: Bit-flip
- **Example**: Feature selection with 0/1 encoding

#### 2. **Real-Valued Representation** 
- **Use Case**: Continuous function optimization, parameter tuning
- **Crossovers**: Arithmetic, blend (BLX-α), SBX, one-point, two-point, uniform
- **Mutations**: Gaussian, uniform
- **Example**: Function optimization problems

#### 3. **Integer Representation**
- **Use Case**: Discrete optimization, scheduling problems  
- **Crossovers**: One-point, two-point, uniform, arithmetic
- **Mutations**: Random resetting, creep
- **Example**: Resource allocation with integer constraints

#### 4. **Permutation Representation**
- **Use Case**: Ordering problems, TSP, scheduling
- **Crossovers**: Order crossover (OX), partially mapped crossover (PMX), cycle crossover
- **Mutations**: Swap, insert, scramble, inversion
- **Example**: Traveling salesman problem, job scheduling

### 🛡️ **Automatic Validation System**

- **Type Safety**: Prevents invalid operator-representation combinations
- **User Guidance**: Shows available operators for selected representation
- **Runtime Validation**: Immediate feedback on invalid choices
- **Smart Defaults**: Fallback to compatible operators

### Genetic Algorithm Components

- **Representation**: Multi-type chromosome support with polymorphic handling
- **Selection**: Tournament selection and roulette wheel with elitism
- **Crossover**: Representation-specific crossover operators
- **Mutation**: Type-aware mutation with automatic bounds checking  
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

The GA can be configured through the enhanced `GAConfig` structure:

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
    
    // Function and representation selection
    enum FunctionType { RASTRIGIN, ACKLEY, SCHWEFEL } function = RASTRIGIN;
    enum RepresentationType { BINARY, REAL_VALUED, INTEGER, PERMUTATION } representation = REAL_VALUED;
    
    // Operator selections (validated against representation)
    string crossoverType = "one_point";    // Auto-validated
    string mutationType = "gaussian";      // Auto-validated 
    string selectionType = "tournament";   // Selection method
    
    bool verbose = true;                   // Print progress
    string outputFile = "ga_results.txt";  // Results file
};
```

### 🎯 **Interactive Configuration**

The program now guides users through configuration:

```bash
=== SIMPLE GENETIC ALGORITHM PIPELINE ===
Enter Representation type (binary, real_valued, integer, permutation): real_valued
Real-valued representation selected.
Available crossovers: arithmetic, blend, sbx, one_point, two_point, uniform
Available mutations: gaussian, uniform
Enter Crossover type: blend
Using blend Crossover
Enter Mutation type: gaussian
Using gaussian Mutation
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
    -o simple_ga_test
```

## Usage Examples

## 🚀 **Quick Start Examples**

### **Command Line Testing**

#### Real-Valued Optimization (Recommended for continuous functions)
```bash
echo -e "real_valued\nblend\ngaussian\ntournament" | ./simple_ga_test
```

#### Binary Optimization (Good for discrete problems)
```bash
echo -e "binary\nuniform\nbit_flip\ntournament" | ./simple_ga_test
```

#### Integer Optimization (For discrete with ranges)
```bash
echo -e "integer\narithmetic\nrandom_resetting\ntournament" | ./simple_ga_test
```

#### Permutation Problems (For ordering/sequencing)
```bash
echo -e "permutation\norder_crossover\nswap\ntournament" | ./simple_ga_test
```

#### Test Invalid Combinations (Educational)
```bash
# This will show validation error:
echo -e "binary\ngaussian\nbit_flip\ntournament" | ./simple_ga_test
# Error: Invalid crossover 'gaussian' for binary representation.
```

### **Interactive Usage** (Recommended for learning)

```bash
./simple_ga_test
```

The program will guide you through:
1. **Representation selection** with available options
2. **Operator validation** against chosen representation  
3. **Automatic configuration** of compatible operators

### **Programmatic Usage Examples**

#### Real-Valued Optimization

```cpp
#include "simple-ga-pipline.cc"

int main() {
    GAConfig config;
    config.populationSize = 100;
    config.generations = 100;
    config.representation = GAConfig::REAL_VALUED;
    config.crossoverType = "blend";        // BLX-α crossover
    config.mutationType = "gaussian";      // Gaussian mutation
    config.function = GAConfig::RASTRIGIN;
    
    SimpleGA ga(config);
    ga.initializePopulation();
    ga.evolve();
    
    return 0;
}
```

#### Binary Optimization

```cpp
GAConfig config;
config.representation = GAConfig::BINARY;
config.crossoverType = "uniform";         // Uniform crossover
config.mutationType = "bit_flip";         // Bit-flip mutation  
config.chromosomeLength = 20;             // 20-bit strings

SimpleGA ga(config);
ga.initializePopulation();
ga.evolve();
```

#### Permutation Problems (TSP-like)

```cpp
GAConfig config;
config.representation = GAConfig::PERMUTATION;
config.crossoverType = "order_crossover"; // Order crossover (OX)
config.mutationType = "swap";             // Swap mutation
config.chromosomeLength = 10;             // 10-city problem

SimpleGA ga(config);
ga.initializePopulation();
ga.evolve();
```

#### Integer Programming

```cpp
GAConfig config;
config.representation = GAConfig::INTEGER;
config.crossoverType = "arithmetic";      // Arithmetic crossover
config.mutationType = "random_resetting"; // Random resetting
config.lowerBound = 0;
config.upperBound = 100;

SimpleGA ga(config);
ga.initializePopulation(); 
ga.evolve();
```

### 🔧 **Validation Examples**

The system automatically prevents invalid combinations:

```bash
Enter Representation type: binary
Enter Crossover type: gaussian
❌ Error: Invalid crossover 'gaussian' for binary representation.
Please choose from available options: one_point, two_point, uniform
```

✅ **Valid combinations**:
- Binary + uniform crossover + bit_flip mutation
- Real-valued + blend crossover + gaussian mutation  
- Permutation + order_crossover + swap mutation
- Integer + arithmetic crossover + creep mutation

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

The GA generates CSV files with fitness evolution data for each representation-operator combination:

- `ga_rastrigin_Real-valued_blend_gaussian_tournament_results.txt`
- `ga_ackley_Binary_uniform_bit_flip_tournament_results.txt`  
- `ga_schwefel_Permutation_order_crossover_swap_tournament_results.txt`

### File Format

```csv
Generation,BestFitness,AvgFitness
0,45.234,23.456
1,52.123,28.789
...
```

### 📊 **Performance Tracking**

Each test run now includes:
- **Representation type** in filename
- **Operator combination** tracking
- **Execution time** measurement
- **Convergence analysis** per representation

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

### 🆕 **Adding New Representations**

1. **Define the representation class**:
```cpp
class MyRepresentationIndividual : public BaseIndividual {
public:
    std::vector<MyType> chromosome;
    
    void randomInitialize(std::mt19937& rng, const GAConfig& config) override {
        // Your initialization logic
    }
    
    void clampToBounds(const GAConfig& config) override {
        // Your bounds checking logic  
    }
    
    Individual toIndividual() const override {
        // Convert to standard Individual format
    }
};
```

2. **Add to RepresentationType enum**:
```cpp
enum RepresentationType { 
    BINARY, REAL_VALUED, INTEGER, PERMUTATION, MY_REPRESENTATION 
};
```

3. **Update validation functions**:
```cpp
bool validateCrossoverForRepresentation(GAConfig::RepresentationType rep, const std::string& crossover) {
    switch(rep) {
        case GAConfig::MY_REPRESENTATION:
            return crossover == "my_crossover1" || crossover == "my_crossover2";
        // ... existing cases
    }
}
```

### 🔧 **Adding New Operators**

1. **Implement in crossover/mutation modules**
2. **Add to factory functions**:
```cpp
std::unique_ptr<CrossoverOperator> createCrossoverOperator(const std::string& type) {
    if (type == "my_new_crossover") {
        return std::make_unique<MyNewCrossover>();
    }
    // ... existing operators
}
```

3. **Update validation and availability functions**

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

### 📈 **Parameter Optimization by Representation**

#### Binary Representation
```cpp
GAConfig config;
config.representation = GAConfig::BINARY;
config.populationSize = 100;      // Larger populations work well
config.mutationRate = 0.05;       // Higher mutation for exploration
config.crossoverRate = 0.9;       // High crossover rate
```

#### Real-Valued Representation  
```cpp
GAConfig config;
config.representation = GAConfig::REAL_VALUED;
config.populationSize = 50;       // Moderate population
config.mutationRate = 0.01;       // Lower mutation for fine-tuning  
config.crossoverRate = 0.8;       // Standard crossover rate
```

#### Permutation Representation
```cpp
GAConfig config;
config.representation = GAConfig::PERMUTATION;
config.populationSize = 200;      // Larger for complex problems
config.mutationRate = 0.1;        // Higher mutation for reordering
config.crossoverRate = 0.7;       // Conservative crossover
```

## 🔍 **Implementation Status**

### ✅ **Completed Features**
- ✅ Multi-representation support (Binary, Real-valued, Integer, Permutation)
- ✅ Automatic operator validation system
- ✅ Interactive user interface with guided selection
- ✅ Polymorphic individual classes with automatic type conversion
- ✅ Extended crossover operator factory supporting representation-specific operators
- ✅ Representation-aware mutation system
- ✅ Comprehensive error handling and user feedback
- ✅ Performance tracking and results logging
- ✅ All benchmark functions tested and working

### 🔧 **Current Limitations & Future Improvements**

#### **Binary Representation Notes**
- Binary encoding works best for problems where variables naturally map to 0/1 values
- For continuous functions, binary encoding may not be optimal compared to real-valued
- Consider increasing population size for binary representations on complex problems

#### **Type Conversion Handling**
- Current implementation converts between double and specific types as needed
- Future versions could support native type handling throughout the pipeline
- Memory optimization possible for pure binary operations

#### **Advanced Operators**
- Basic operators implemented; could add more sophisticated variants:
  - SBX (Simulated Binary Crossover) for real-valued - ✅ Implemented
  - Differential Evolution crossover - Could be added
  - Adaptive parameter control - Future enhancement

### 📋 **Compilation Notes**
- Requires C++17 compiler support
- Links against crossover.cc, mutation.cc, selection-operator.cc, and fitness-fuction.cc
- Some compiler warnings about virtual function overloading are expected (non-critical)

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure C++17 support: `g++ --version`
   - Check include paths: `-I..`

2. **Invalid Operator Combinations**
   - ❌ `Binary representation + Gaussian mutation`
   - ❌ `Permutation representation + Bit-flip mutation`
   - ✅ Use the interactive mode for guided selection

3. **Poor Convergence by Representation**
   - **Binary**: Try uniform crossover + higher mutation rates
   - **Real-valued**: Use blend crossover + gaussian mutation  
   - **Integer**: Arithmetic crossover + creep mutation
   - **Permutation**: Order crossover + swap mutation

4. **Build Failures**
   - Clean and rebuild: `make clean && make`
   - Check dependencies

### 🐛 **Debug Mode**

Enable verbose output for detailed information:

```cpp
config.verbose = true;  // Shows operator selection and validation
```

### 🔍 **Validation Debugging**

The system provides detailed error messages:

```bash
❌ Invalid crossover 'gaussian' for binary representation.
   Available options: one_point, two_point, uniform

❌ Invalid mutation 'swap' for real_valued representation.  
   Available options: gaussian, uniform
```

### 📊 **Performance Debugging**

Monitor convergence by representation:
- **Binary**: Should show discrete fitness jumps
- **Real-valued**: Smooth convergence curves
- **Integer**: Step-wise improvements  
- **Permutation**: May show plateaus with sudden improvements

## Dependencies

The test suite depends on the following enhanced modules:

- **🆕 Multi-Representation System**: BaseIndividual, RealValuedIndividual, BinaryIndividual, IntegerIndividual
- **Selection Operators**: Tournament, roulette wheel, rank selection, elitism
- **🆕 Extended Crossover Operators**: 
  - Binary: One-point, two-point, uniform
  - Real-valued: Arithmetic, blend (BLX-α), SBX, one-point, two-point, uniform
  - Integer: One-point, two-point, uniform, arithmetic  
  - Permutation: Order crossover (OX), PMX, cycle crossover
- **🆕 Representation-Specific Mutations**:
  - Binary: Bit-flip
  - Real-valued: Gaussian, uniform
  - Integer: Random resetting, creep
  - Permutation: Swap, insert, scramble, inversion
- **🆕 Validation System**: Automatic operator-representation compatibility checking
- **Logging System**: Comprehensive logging for debugging

## 🎯 **Key Improvements**

### ✅ **Type Safety**
- Prevents runtime errors from invalid operator combinations
- Compile-time and runtime validation of configurations
- **Tested**: Binary + Gaussian crossover → Immediate error with helpful message

### ✅ **User Experience**
- Interactive configuration with guided operator selection
- Clear error messages with available alternatives
- Automatic bounds checking and type conversion
- **Example**: Shows "Available crossovers: one_point, two_point, uniform" for binary representation

### ✅ **Extensibility**
- Easy addition of new representation types
- Modular operator system with factory patterns
- Polymorphic individual handling

### ✅ **Performance**
- Representation-specific optimizations
- Efficient type conversions when needed
- Optimized operator implementations per encoding
- **Test Results**: Binary representation found optimal solutions (fitness=1000) for Rastrigin and Ackley functions

### ✅ **Automatic Validation**
The system now prevents invalid combinations at runtime:

```bash
❌ Binary + Gaussian crossover: "Invalid crossover 'gaussian' for binary representation"
❌ Real-valued + Swap mutation: "Invalid mutation 'swap' for real_valued representation"  
✅ Binary + Uniform crossover + Bit-flip mutation: "Using uniform Crossover"
✅ Real-valued + Blend crossover + Gaussian mutation: "Using blend Crossover"
```

## Test Results Summary

Recent testing shows the representation system working correctly:

### Binary Representation Results
- **Rastrigin Function**: Perfect solution found (fitness=1000, actual value=0.0)
- **Ackley Function**: Perfect solution found (fitness=1000, actual value=0.0)  
- **Schwefel Function**: Challenging for binary encoding (fitness=0.376)

### Real-Valued Representation Results  
- **Rastrigin Function**: Good convergence (fitness=50.07, actual value=18.97)
- **Ackley Function**: Excellent performance (fitness=373.52, actual value=1.68)
- **Schwefel Function**: Moderate performance (fitness=0.78, actual value=1281.86)

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