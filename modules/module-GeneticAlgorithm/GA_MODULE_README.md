# MBDyn Genetic Algorithm Optimization Module

This module implements a hybrid UDE (User Defined Element) + Shared GA approach for genetic algorithm optimization within MBDyn simulations.

## Architecture

```
MBDyn → UDE wrapper → galib.so → GA optimization → back to UDE → MBDyn
                   → libcons.so → constraint checking
```

The GA lives in `galib.so` (independent, reusable), while MBDyn has a UDE wrapper that loads and calls GA functions.

## Usage in .mbd Files

### Basic Syntax

```
user defined: <label>, genetic algorithm optimization,
    inputs number, <N>,
        <drive_spec_1>,
        <drive_spec_2>,
        ...
        <drive_spec_N>,
    genetic algorithm: <path_to_galib.so>,
    constraint function: <path_to_libcons.so>,
    population, <pop_size>,
    generation, <generations>,
    output number, <M>,
        <output_label_1>,
        <output_label_2>,
        ...
        <output_label_M>;
```

### Example

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
    generation, 100,
    output number, 3,
        121,
        123,
        126;
```

## Input Drives

Input drives can be any valid MBDyn drive specification:
- **Node data**: `node, <id>, structural, string, "<quantity>", direct`
  - Quantities: `"X[1]"`, `"X[2]"`, `"X[3]"` (positions)
  - Quantities: `"XP[1]"`, `"XP[2]"`, `"XP[3]"` (velocities)
  - Quantities: `"XPP[1]"`, `"XPP[2]"`, `"XPP[3]"` (accelerations)
- **Element data**: `element, <id>, <type>, string, "<private_data>", direct`
  - Examples: `"M[1]"`, `"M[2]"`, `"M[3]"` (moments from joints)
  - Examples: `"F[1]"`, `"F[2]"`, `"F[3]"` (forces)
- **Constants**: `const, <value>`
- **Expressions**: `mult, <factor>, <inner_drive>`

## Output Data Access

The GA module exposes optimized results via private data:

### Private Data Names

- `"fitness"` - Current best fitness value from GA
- `"output[k]"` - kth optimized output (1-based indexing)
- `"timestep"` - Alias for `"output[1]"` (convenience for timestep control)

### Example Usage

```
# Use GA fitness in a force
force: 100, absolute,
    2,
        position, reference, node, null,
    1., 0., 0.,
        element, 1000, loadable, string, "fitness", linear, 0., 1.;

# Use optimized outputs
force: 101, absolute,
    121,
        position, reference, node, null,
    1., 0., 0.,
        element, 1000, loadable, string, "output[1]", linear, 0., 1.;
```

## GA Library API

Your `galib.so` must export these C functions:

### Required Functions

```c
// Initialize GA context
void* ga_init(int population_size, int generations);

// Run one generation/step of GA
void ga_run(void* ga_ctx);

// Get current best fitness
double ga_get_best(void* ga_ctx);
```

### Optional Functions (for input/output handling)

```c
// Set input variables for current evaluation
void ga_set_inputs(void* ga_ctx, const double* inputs, int n_inputs);

// Get optimized output variables
void ga_get_outputs(void* ga_ctx, double* outputs, int n_outputs);

// Clean up GA context
void ga_free(void* ga_ctx);
```

## Constraint Library API

Your `libcons.so` (optional) can export constraint checking functions:

```c
// Check constraints for given solution
bool constraint_check(const double* solution, int n_vars);

// Get constraint violation measure
double constraint_violation(const double* solution, int n_vars);
```

## Implementation Flow

1. **Initialization**: 
   - Parse input drives and output specifications
   - Load `galib.so` and optional `libcons.so`
   - Initialize GA context with population and generation parameters

2. **Runtime (each simulation step)**:
   - Evaluate all input drives to get current input variable values
   - Pass inputs to GA via `ga_set_inputs()` (if available)
   - Run GA step via `ga_run()`
   - Retrieve best fitness via `ga_get_best()`
   - Retrieve optimized outputs via `ga_get_outputs()` (if available)
   - Cache results for private data access

3. **Output**: 
   - Other elements can access GA results via private data drives
   - Results can be used for forces, constraints, control, etc.

## Building

1. Ensure your module is in the MBDyn modules directory
2. Configure MBDyn to include the module
3. Build with standard MBDyn build process

```bash
# In MBDyn root directory
./configure --enable-modules
make
```

## Testing

See `test_ga_optimization.mbd` for a complete example with:
- 5 input drives from joint moments
- GA library and constraint function specified
- 3 output variables
- Population of 50, 100 generations

## Notes

- Input drives are evaluated every simulation step, providing real-time data to the GA
- Output variables are cached and available immediately via private data
- The GA library is independent of MBDyn and can be reused in other contexts
- Constraint functions are optional but recommended for constrained optimization problems
- All file paths for libraries are relative to MBDyn's working directory