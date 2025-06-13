# MBDyn Genetic Algorithm Module - Testing Documentation

## Quick Start

To run all tests:
```bash
cd /home/rahul/MBDyn-GA/MBDyn/tests/GeneticAlgorithmOptimization
make all
```

To run specific test categories:
```bash
make unit          # Unit tests only
make integration   # Integration tests only  
make system        # System tests only
```

## Test Categories

### 1. Unit Tests

Unit tests validate individual GA operators in isolation:

- **Selection Operators**: Tournament, roulette wheel, rank-based, SUS, elitism
- **Crossover Operators**: Single-point, two-point, uniform, adaptive
- **Mutation Operators**: Gaussian, uniform, swap, inversion
- **Hill Climbing**: Steepest ascent, stochastic, random restart

Run with: `make unit`

### 2. Integration Tests

Integration tests validate MBDyn connectivity:

- **Element Modification**: Beam, mass, joint, damper parameter updates
- **Output Monitoring**: Node displacement, force, stress monitoring
- **Fitness Evaluation**: Expression parsing, constraint handling

Run with: `make integration`

### 3. System Tests

System tests validate complete optimization workflows:

- **Beam Optimization**: Cantilever beam design optimization
- **Truss Optimization**: Multi-member truss structure optimization
- **Multi-Objective**: Pareto front generation for conflicting objectives

Run with: `make system`

## Test Structure

```
tests/GeneticAlgorithmOptimization/
├── unit_tests/                 # Unit test implementations
│   ├── test_framework.h       # Testing utilities and base classes
│   ├── test_selection.cc      # Selection operator tests
│   ├── test_crossover.cc      # Crossover operator tests
│   ├── test_mutation.cc       # Mutation operator tests
│   └── test_hill_climbing.cc  # Hill climbing tests
├── integration_tests/          # Integration test files
│   ├── test_beam_optimization.mbd
│   ├── test_truss_optimization.mbd
│   └── test_multi_objective.mbd
├── system_tests/              # End-to-end system tests
├── benchmarks/                # Performance benchmark tests
├── validation/                # Validation against known solutions
└── scripts/                   # Test automation scripts
```

## Performance Testing

### Benchmark Tests
```bash
make performance
```

Measures performance of GA operators under different conditions:
- Population sizes: 10, 50, 100, 500, 1000
- Problem dimensions: 2, 5, 10, 20, 50 variables
- Generation counts: 10, 50, 100, 500 generations

### Memory Testing
```bash
make memcheck
```

Uses Valgrind to detect memory leaks and invalid memory access.

### Stress Testing
```bash
make stress
```

Runs tests repeatedly to detect race conditions and intermittent failures.

## Coverage Analysis

Generate code coverage report:
```bash
make coverage
```

This creates an HTML coverage report in `coverage_html/index.html`.

Target coverage levels:
- Unit tests: >95%
- Integration tests: >85%
- System tests: >75%

## Continuous Integration

### Fast CI (for pull requests):
```bash
make ci-fast
```

### Full CI (for merges):
```bash
make ci-full
```

### Nightly CI (comprehensive):
```bash
make ci-nightly
```

## Test Data Management

### Reference Solutions
Located in `test_data/reference_solutions/`:
- Analytical solutions for simple problems
- Verified numerical solutions for complex problems
- Expected convergence behavior data

### Benchmark Problems
Located in `test_data/benchmark_problems/`:
- Standard optimization test functions
- Engineering optimization problems
- Multi-objective test suites

### Validation Datasets
Located in `test_data/validation_datasets/`:
- Experimental data for validation
- Literature results for comparison
- Performance baseline data

## Writing New Tests

### Unit Test Template

```cpp
#include "test_framework.h"

bool test_new_feature() {
    // Setup test data
    Population pop(50, 5);
    pop.evaluate(TestFunctions::sphere);
    
    // Test the feature
    auto result = new_feature(pop);
    
    // Validate results
    TEST_ASSERT(result.is_valid());
    TEST_ASSERT_NEAR(result.fitness, expected_value, tolerance);
    
    return true;
}

int main() {
    int tests_passed = 0, tests_failed = 0, tests_total = 0;
    
    RUN_TEST(test_new_feature);
    
    std::cout << "Results: " << tests_passed << "/" << tests_total << " passed" << std::endl;
    return tests_failed == 0 ? 0 : 1;
}
```

### Integration Test Template

```plaintext
# filepath: test_new_integration.mbd
begin: data;
    problem: initial value;
end: data;

begin: initial value;
    initial time: 0.0;
    final time: 1.0;
    time step: 0.01;
end: initial value;

begin: control data;
    structural nodes: 2;
    user defined elements: 1;
end: control data;

begin: nodes;
    structural: 1, static, null, eye, null, null;
    structural: 2, dynamic, 1., 0., 0., eye, null, null;
end: nodes;

begin: elements;
    user defined: 1000, genetic algorithm,
        inputs: 1,
        elements: 1,
        element types: "beam",
        data names: "stiffness",
        outputs: 1,
        output drives: 100,
        fitness function: "minimize(displacement)",
        constraints function: "stiffness > 1000",
        population size: 10,
        generations: 20;
end: elements;
```

## Troubleshooting

### Common Issues

1. **Test Compilation Errors**
   - Check include paths in Makefile
   - Verify MBDyn installation
   - Update compiler flags if needed

2. **Test Execution Failures**
   - Check file permissions
   - Verify test data availability
   - Review error logs in test output

3. **Performance Issues**
   - Monitor memory usage during tests
   - Check for infinite loops in GA operators
   - Verify termination criteria

4. **Inconsistent Results**
   - Set random seeds for reproducibility
   - Check floating-point precision issues
   - Verify statistical significance

### Debug Mode
Run tests with debug output:
```bash
make unit CXXFLAGS="-DDEBUG -g"
```

### Verbose Output
Get detailed test information:
```bash
make unit VERBOSE=1
```

## Test Metrics and Reporting

### Success Criteria
- **Unit Tests**: 100% pass rate, <1s execution time per test
- **Integration Tests**: 100% pass rate, <30s execution time per test  
- **System Tests**: Convergence to within 5% of known optimum

### Automated Reporting
Test reports are automatically generated with:
- Pass/fail status for each test
- Performance metrics and trends
- Coverage analysis results
- Regression detection alerts

### Dashboard Integration
Results are integrated into CI/CD dashboard showing:
- Test status across all branches
- Performance trends over time
- Coverage evolution
- Failure rate statistics

## Validation Against Literature

The test suite includes validation against published results:

1. **Optimization Benchmarks**
   - CEC benchmark functions
   - Engineering optimization problems from literature
   - Multi-objective test problems (ZDT, DTLZ suites)

2. **Structural Optimization**
   - Cantilever beam optimization (Bendsøe & Sigmund)
   - Truss optimization problems (Michell structures)
   - Frequency optimization (modal analysis validation)

3. **Algorithm Performance**
   - Convergence rate comparisons
   - Solution quality assessments
   - Computational efficiency benchmarks

## Contributing to Tests

When adding new features to the GA module:

1. **Write unit tests first** (test-driven development)
2. **Add integration tests** for MBDyn connectivity
3. **Include system tests** for complete workflows
4. **Update documentation** with test descriptions
5. **Verify performance** doesn't regress
6. **Add validation data** for new problem types

### Pull Request Requirements

All pull requests must:
- Pass all existing tests
- Include tests for new functionality
- Maintain >95% unit test coverage
- Include documentation updates
- Pass performance benchmarks
