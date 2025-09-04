/*
 * Interface for libcons.so - Constraint Library  
 * This library contains constraint checking functions for genetic algorithm
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Check if a genome satisfies all constraints
 * genome: array of parameter values to check
 * genomeLen: number of parameters in genome
 * Returns: 1 if constraints satisfied, 0 if violated
 */
int constraint_ok(const double* genome, int genomeLen);

/* Get constraint violation penalty
 * genome: array of parameter values
 * genomeLen: number of parameters
 * Returns: penalty value (0 = no violation, higher = worse violation)
 */
double constraint_penalty(const double* genome, int genomeLen);

/* Check bounds constraints
 * genome: parameter values to check
 * lower_bounds: array of lower bounds for each parameter
 * upper_bounds: array of upper bounds for each parameter  
 * genomeLen: number of parameters
 * Returns: 1 if all bounds satisfied, 0 otherwise
 */
int check_bounds(const double* genome, const double* lower_bounds, 
                 const double* upper_bounds, int genomeLen);

/* Get detailed constraint information
 * genome: parameter values to check
 * genomeLen: number of parameters
 * violations: output array indicating which constraints are violated
 * num_violations: output - number of constraint violations
 * Returns: 1 if all constraints satisfied, 0 otherwise
 */
int constraint_details(const double* genome, int genomeLen,
                      int* violations, int* num_violations);

#ifdef __cplusplus
}
#endif
