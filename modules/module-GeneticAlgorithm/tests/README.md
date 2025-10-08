# Unit Tests for Genetic Algorithm Module

This directory contains comprehensive unit tests for the MBDyn Genetic Algorithm module using Google Test.

## Test Structure

### Test Files

1. **`test_genetic_algorithm.cc`** - Main test file containing:
   - Tests for utility functions (`clamp01`)
   - Tests for `GAConfig` structure
   - Integration tests with mock GA library
   - Error handling tests
   - Performance benchmarks (disabled by default)

2. **`test_ga_utilities.cc`** - Tests for GA utility functions:
   - Probability normalization
   - Selection algorithms (roulette wheel, tournament)
   - Crossover operations (single-point)
   - Mutation operations (bit-flip)

3. **`mock_ga_library.cc`** - Mock implementation of the GA library:
   - Simulates the external GA library that the module loads via `dlopen`
   - Provides predictable behavior for testing
   - Includes additional functions for extended testing

### Build Configuration

- **`Makefile.am`** - Autotools configuration for building tests
- Integrates with MBDyn's existing build system
- Links with Google Test and required MBDyn libraries

## Building and Running Tests

### Prerequisites

1. Ensure Google Test is installed and available in the MBDyn build environment
2. Configure MBDyn with test support enabled

### Build Commands

```bash
# From the MBDyn root directory
./configure --enable-tests  # or whatever test flags are needed
make
cd modules/module-GeneticAlgorithm/tests
make check
```

### Running Individual Tests

```bash
# Run all tests
./test_genetic_algorithm

# Run specific test suites
./test_genetic_algorithm --gtest_filter="GAUtilityTest.*"
./test_genetic_algorithm --gtest_filter="SelectionTest.*"

# Run with verbose output
./test_genetic_algorithm --gtest_verbose

# Run performance tests (normally disabled)
./test_genetic_algorithm --gtest_also_run_disabled_tests --gtest_filter="*Performance*"
```

## Test Categories

### 1. Unit Tests
- Test individual functions and components in isolation
- Use mock objects to eliminate external dependencies
- Fast execution, suitable for frequent running during development

### 2. Integration Tests
- Test interaction between module components
- Simulate complete GA workflows
- Verify correct behavior with mock GA library

### 3. Error Handling Tests
- Test behavior with invalid inputs
- Verify proper error reporting and recovery
- Ensure graceful degradation

### 4. Performance Tests
- Benchmark GA operations
- Monitor memory usage and execution time
- Disabled by default (enable with `--gtest_also_run_disabled_tests`)

## Mock GA Library

The mock library (`mock_ga_library.cc`) provides:

- **Predictable Results**: Consistent behavior for reproducible tests
- **Configurable Responses**: Simulate different GA scenarios
- **Extended API**: Additional functions for comprehensive testing
- **No External Dependencies**: Self-contained implementation

### Mock Functions

- `mock_ga_init()` - Initialize GA context
- `mock_ga_run()` - Simulate GA execution with improving fitness
- `mock_ga_get_best()` - Return current best fitness
- `mock_ga_cleanup()` - Clean up resources
- Additional utilities for population analysis

## Test-Driven Development (TDD)

This test suite supports TDD workflow:

1. **Write tests first** - Define expected behavior before implementation
2. **Implement incrementally** - Add module functionality to pass tests
3. **Refactor safely** - Tests ensure changes don't break existing functionality
4. **Continuous validation** - Run tests frequently during development

## Adding New Tests

### For New GA Functions

1. Add function declarations to the appropriate test file
2. Create test fixtures if needed for setup/teardown
3. Write comprehensive test cases covering:
   - Normal operation
   - Edge cases
   - Error conditions
   - Performance characteristics

### Example Test Addition

```cpp
// In test_ga_utilities.cc
TEST_F(SelectionTest, RankSelection) {
    // Test rank-based selection
    std::vector<int> ranks = {1, 2, 3, 4};  // Higher rank = better
    
    // Test implementation here
    EXPECT_GT(selected_high_rank_count, selected_low_rank_count);
}
```

## Continuous Integration

These tests are designed to integrate with MBDyn's CI pipeline:

- Fast execution for quick feedback
- Comprehensive coverage for reliability
- Clear output for debugging failures
- Configurable verbosity levels

## Troubleshooting

### Common Issues

1. **Missing Google Test**: Ensure `GTEST_CFLAGS` and `GTEST_LIBS` are properly set
2. **Linking Errors**: Verify MBDyn libraries are built and accessible
3. **Mock Library Issues**: Check that mock functions match expected signatures

### Debug Options

```bash
# Verbose test output
./test_genetic_algorithm --gtest_verbose

# Filter specific failing tests
./test_genetic_algorithm --gtest_filter="*FailingTest*"

# Repeat tests to catch intermittent issues
./test_genetic_algorithm --gtest_repeat=10
```

## Future Enhancements

- **Property-based testing** - Generate random inputs for robust validation
- **Mutation testing** - Verify test quality by introducing artificial bugs
- **Coverage analysis** - Ensure comprehensive test coverage
- **Stress testing** - Test with large populations and generations
- **Real GA integration** - Test with actual GA library when available