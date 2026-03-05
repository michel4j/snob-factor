# Minimum Message Length (MML) Mixture Modelling in Snob-Factor

This report details how the Minimum Message Length (MML) algorithms are implemented in the `snob-factor` codebase. The system performs unsupervised classification (mixture modelling) by finding a tree-structured set of classes (clusters) that minimizes the total message length of describing both the model parameters and the data itself given the model.

## 1. Core Concepts and Objectives

In MML, a model is evaluated based on the length of a hypothetical encoded message that perfectly transmits the dataset. The total message length (cost) is:
```text
Total Message Length = Model Length + Data Length
```
- **Model Length (Parameter Cost)**: The cost of encoding the number of classes, the tree structure linking them, and the parameters (e.g., means and standard deviations for real variables, probabilities for discrete variables) to a precision optimized by MML principles. In the codebase, this is accumulated in variables like `cls->best_par_cost`.
- **Data Length (Case Cost)**: The cost to encode the individual data cases under the given model. The better a data point fits a class's distribution, the shorter its encoded length (`cls->best_case_cost`).

The `Result classify` function inside [src/glob.c](file:///home/michel/Projects/snob-factor/src/glob.c) is the high-level loop that minimizes this total message length.

## 2. Model Structure (Hierarchical Clustering)

Unlike flat mixture models (like k-means), `snob-factor` uses a hierarchy of classes:
- **Root**: The top-level population model.
- **Dads**: Internal nodes that act as prior distributions for their children. By having a "Dad" class, the code compresses the description of similar "Leaf" parameters.
- **Leaves**: The actual mixture components. Data items are primarily assigned to the leaves.
- **Subs**: Experimental sub-classes used to test if a leaf should be split.

## 3. The Algorithm Execution

The [classify()](file:///home/michel/Projects/snob-factor/src/glob.c#281-365) function continually alternates between two primary phases until the MML cost no longer decreases:
1. **Parameter Estimation ([do_all](file:///home/michel/Projects/snob-factor/src/doall.c#415-477))**: Assigns data to classes and optimizes continuous parameters.
2. **Structural Search ([try_moves](file:///home/michel/Projects/snob-factor/src/tactics.c#807-835), [tactics.c](file:///home/michel/Projects/snob-factor/src/tactics.c))**: Changes the discrete structure of the hierarchy (e.g., splitting, merging, moving).

### Phase A: [do_all](file:///home/michel/Projects/snob-factor/src/doall.c#415-477) - Assignment and Parameter Optimization

Implemented in [src/doall.c](file:///home/michel/Projects/snob-factor/src/doall.c), this phase is analogous to an Expectation-Maximization (EM) cycle, but adapted for MML:

#### Assignment ([do_case](file:///home/michel/Projects/snob-factor/src/doall.c#643-883))
For each item in the dataset, the code evaluates its cost against all potential classes.
- For a real variable (in [src/reals.c](file:///home/michel/Projects/snob-factor/src/reals.c)), the case cost is $-\log P(x | \mu, \sigma) + \text{cost of encoding latent factors}$. 
- The case is assigned soft weights (`cls->case_weight`) across leaves. The weights are calculated proportionally to $\exp(-\text{cost})$.

#### Estimation ([adjust_class](file:///home/michel/Projects/snob-factor/src/classes.c#498-656))
Once all items have calculated their weights for all classes, the parameters of the classes are updated to minimize the local MML cost:
- Instead of simple Maximum Likelihood estimates, the parameters incorporate prior information from their parent (`Dad`) class.
- The parameter "spread" (which determines the coding precision) is adapted such that parameters are stated optimally (e.g., standard deviation precision $\propto 1/\sqrt{N}$).

### Phase B: Structural Search Tactics ([tactics.c](file:///home/michel/Projects/snob-factor/src/tactics.c))

The code employs heuristic moves to adjust the hierarchy. If a move reduces the total MML cost, it is accepted.
- **Split Leaf ([split_leaf](file:///home/michel/Projects/snob-factor/src/classes.c#736-758))**: Converts a large, diffuse leaf into a Dad with Subclass leaves.
- **Insert Dad ([insert_dad](file:///home/michel/Projects/snob-factor/src/tactics.c#56-154))**: Selects two classes that are structurally sibling nodes and groups them under a newly created "Dad" class. This is accepted if the cost of stating the new Dad's parameters is offset by the savings in specifying the two children's parameters as small offsets from the new Dad.
- **Remove/Splice Dad ([splice_dad](file:///home/michel/Projects/snob-factor/src/tactics.c#312-352))**: Deletes a Dad, promoting its children upwards if the shared parameter compression is no longer worth the cost of the intermediate node.
- **Move Class ([move_class](file:///home/michel/Projects/snob-factor/src/tactics.c#583-648))**: Detaches a class and reattaches it to a different Dad.

## 4. Main Pseudocode

The following pseudocode synthesizes the core logic behind the [classify](file:///home/michel/Projects/snob-factor/src/glob.c#281-365) process:

```python
def classify(dataset, max_cycles, tol):
    initialize_population_with_root(dataset)
    current_cost = Infinity
    
    for cycle in range(max_cycles):
        # 1. Parameter Estimation and Assignment Phase
        for step in range(do_steps):
            for item in dataset:
                # E-step equivalent: Calculate weights for each leaf
                costs = calculate_case_costs_for_all_leaves(item)
                assign_soft_weights_to_leaves(item, costs)
                
            for class_node in population:
                # M-step equivalent: Re-estimate parameters and coding precision
                adjust_class_parameters(class_node)
                
        # 2. Structural Search Phase
        attempt_structural_moves(move_steps)
        
        # 3. Evaluate Convergence
        new_cost = calculate_total_mml_cost(population)
        if (current_cost - new_cost) / current_cost < tol:
            break
        current_cost = new_cost
        
    return population

def attempt_structural_moves(move_steps):
    for move_attempt in range(move_steps):
        best_drop = -Infinity
        best_move = None
        
        # Try all possible dad insertions
        for (class1, class2) in get_all_sibling_pairs():
            cost_drop = evaluate_insert_dad(class1, class2)
            if cost_drop > best_drop:
                best_drop = cost_drop
                best_move = ("insert_dad", class1, class2)
                
        # Try all possible class moves
        for (child, possible_new_dad) in get_all_child_dad_combinations():
            cost_drop = evaluate_move_class(child, possible_new_dad)
            if cost_drop > best_drop:
                best_drop = cost_drop
                best_move = ("move_class", child, possible_new_dad)
                
        # Try dad deletions
        for dad in get_all_dads():
            cost_drop = evaluate_remove_dad(dad)
            if cost_drop > best_drop:
                best_drop = cost_drop
                best_move = ("remove_dad", dad)
                
        if best_drop > 0:
            apply_move(best_move)
        else:
            break
```

## Summary

The program achieves MML mixture modelling by representing the clusters as a hierarchy and repeatedly alternating between estimating parameters (using soft assignments derived from MML cost evaluations) and mutating the tree topology. By encoding similar clusters under a shared parent, it inherently discovers and compresses redundant features across subgroups.
