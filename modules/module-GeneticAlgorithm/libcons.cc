/*
 * Constraint/fitness library for GA module
 * Provides fitness evaluation function called by the UDE
 */

#include <cmath>

extern "C" {
    // Main fitness function - evaluates quality of a solution
    // genes: chromosome values
    // n_genes: number of genes
    // inputs: optional conditioning inputs from MBDyn drives
    // n_inputs: number of inputs
    // Returns: fitness value (higher is better - GA maximizes)
    double evaluate_fitness(const double* genes, int n_genes, 
                           const double* inputs, int n_inputs) {
        // Example: Rastrigin function (minimization problem)
        // We negate it so GA can maximize
        double fitness = 0.0;
        for (int i = 0; i < n_genes; ++i) {
            fitness += genes[i] * genes[i] - 10.0 * cos(2.0 * M_PI * genes[i]) + 10.0;
        }
        return -fitness; // Negative because GA maximizes
    }
    
    // Simple wrapper without inputs
    double evaluate_fitness_simple(const double* genes, int n_genes) {
        return evaluate_fitness(genes, n_genes, nullptr, 0);
    }
    
    // Constraint violation check
    // Returns: 0 if satisfied, positive penalty if violated
    double evaluate_constraint(const double* genes, int n_genes) {
        double penalty = 0.0;
        
        // Example: penalize if any gene exceeds [-10, 10]
        for (int i = 0; i < n_genes; ++i) {
            if (genes[i] < -10.0) penalty += (-10.0 - genes[i]);
            if (genes[i] > 10.0) penalty += (genes[i] - 10.0);
        }
        
        return penalty;
    }
}
