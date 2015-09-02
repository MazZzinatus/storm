#include "src/modelchecker/prctl/helper/HybridMdpPrctlHelper.h"

#include "src/storage/dd/CuddDdManager.h"
#include "src/storage/dd/CuddAdd.h"
#include "src/storage/dd/CuddBdd.h"
#include "src/storage/dd/CuddOdd.h"

#include "src/utility/graph.h"
#include "src/utility/constants.h"

#include "src/models/symbolic/StandardRewardModel.h"

#include "src/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "src/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "src/modelchecker/results/HybridQuantitativeCheckResult.h"

#include "src/solver/MinMaxLinearEquationSolver.h"

#include "src/exceptions/InvalidPropertyException.h"

namespace storm {
    namespace modelchecker {
        namespace helper {
            
            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates, bool qualitative, storm::utility::solver::MinMaxLinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
                // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
                // probability 0 and 1 of satisfying the until-formula.
                std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;
                if (dir == OptimizationDirection::Minimize) {
                    statesWithProbability01 = storm::utility::graph::performProb01Min(model, phiStates, psiStates);
                } else {
                    statesWithProbability01 = storm::utility::graph::performProb01Max(model, phiStates, psiStates);
                }
                storm::dd::Bdd<DdType> maybeStates = !statesWithProbability01.first && !statesWithProbability01.second && model.getReachableStates();
                
                // Perform some logging.
                STORM_LOG_INFO("Found " << statesWithProbability01.first.getNonZeroCount() << " 'no' states.");
                STORM_LOG_INFO("Found " << statesWithProbability01.second.getNonZeroCount() << " 'yes' states.");
                STORM_LOG_INFO("Found " << maybeStates.getNonZeroCount() << " 'maybe' states.");
                
                // Check whether we need to compute exact probabilities for some states.
                if (qualitative) {
                    // Set the values for all maybe-states to 0.5 to indicate that their probability values are neither 0 nor 1.
                    return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), statesWithProbability01.second.toAdd() + maybeStates.toAdd() * model.getManager().getConstant(0.5)));
                } else {
                    // If there are maybe states, we need to solve an equation system.
                    if (!maybeStates.isZero()) {
                        // Create the ODD for the translation between symbolic and explicit storage.
                        storm::dd::Odd<DdType> odd(maybeStates);
                        
                        // Create the matrix and the vector for the equation system.
                        storm::dd::Add<DdType> maybeStatesAdd = maybeStates.toAdd();
                        
                        // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
                        // non-maybe states in the matrix.
                        storm::dd::Add<DdType> submatrix = transitionMatrix * maybeStatesAdd;
                        
                        // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
                        // maybe states.
                        storm::dd::Add<DdType> prob1StatesAsColumn = statesWithProbability01.second.toAdd();
                        prob1StatesAsColumn = prob1StatesAsColumn.swapVariables(model.getRowColumnMetaVariablePairs());
                        storm::dd::Add<DdType> subvector = submatrix * prob1StatesAsColumn;
                        subvector = subvector.sumAbstract(model.getColumnVariables());
                        
                        // Before cutting the non-maybe columns, we need to compute the sizes of the row groups.
                        std::vector<uint_fast64_t> rowGroupSizes = submatrix.notZero().existsAbstract(model.getColumnVariables()).toAdd().sumAbstract(model.getNondeterminismVariables()).template toVector<uint_fast64_t>(odd);
                        
                        // Finally cut away all columns targeting non-maybe states.
                        submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());
                        
                        // Create the solution vector.
                        std::vector<ValueType> x(maybeStates.getNonZeroCount(), ValueType(0.5));
                        
                        // Translate the symbolic matrix/vector to their explicit representations and solve the equation system.
                        std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation = submatrix.toMatrixVector(subvector, std::move(rowGroupSizes), model.getNondeterminismVariables(), odd, odd);
                        
                        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(explicitRepresentation.first);
                        solver->solveEquationSystem(dir, x, explicitRepresentation.second);
                        
                        // Return a hybrid check result that stores the numerical values explicitly.
                        return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), model.getReachableStates() && !maybeStates, statesWithProbability01.second.toAdd(), maybeStates, odd, x));
                    } else {
                        return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), statesWithProbability01.second.toAdd()));
                    }
                }
            }

            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeNextProbabilities(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, storm::dd::Bdd<DdType> const& nextStates) {
                storm::dd::Add<DdType> result = transitionMatrix * nextStates.swapVariables(model.getRowColumnMetaVariablePairs()).toAdd();
                return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), result.sumAbstract(model.getColumnVariables())));
            }

            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeBoundedUntilProbabilities(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates, uint_fast64_t stepBound, storm::utility::solver::MinMaxLinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
                // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
                // probability 0 or 1 of satisfying the until-formula.
                storm::dd::Bdd<DdType> statesWithProbabilityGreater0;
                if (dir == OptimizationDirection::Minimize) {
                    statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0A(model, transitionMatrix.notZero(), phiStates, psiStates);
                } else {
                    statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0E(model, transitionMatrix.notZero(), phiStates, psiStates);
                }
                storm::dd::Bdd<DdType> maybeStates = statesWithProbabilityGreater0 && !psiStates && model.getReachableStates();
                
                // If there are maybe states, we need to perform matrix-vector multiplications.
                if (!maybeStates.isZero()) {
                    // Create the ODD for the translation between symbolic and explicit storage.
                    storm::dd::Odd<DdType> odd(maybeStates);
                    
                    // Create the matrix and the vector for the equation system.
                    storm::dd::Add<DdType> maybeStatesAdd = maybeStates.toAdd();
                    
                    // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
                    // non-maybe states in the matrix.
                    storm::dd::Add<DdType> submatrix = transitionMatrix * maybeStatesAdd;
                    
                    // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
                    // maybe states.
                    storm::dd::Add<DdType> prob1StatesAsColumn = psiStates.toAdd().swapVariables(model.getRowColumnMetaVariablePairs());
                    storm::dd::Add<DdType> subvector = (submatrix * prob1StatesAsColumn).sumAbstract(model.getColumnVariables());
                    
                    // Before cutting the non-maybe columns, we need to compute the sizes of the row groups.
                    std::vector<uint_fast64_t> rowGroupSizes = submatrix.notZero().existsAbstract(model.getColumnVariables()).toAdd().sumAbstract(model.getNondeterminismVariables()).template toVector<uint_fast64_t>(odd);
                    
                    // Finally cut away all columns targeting non-maybe states.
                    submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());
                    
                    // Create the solution vector.
                    std::vector<ValueType> x(maybeStates.getNonZeroCount(), storm::utility::zero<ValueType>());
                    
                    // Translate the symbolic matrix/vector to their explicit representations.
                    std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation = submatrix.toMatrixVector(subvector, std::move(rowGroupSizes), model.getNondeterminismVariables(), odd, odd);
                    
                    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(explicitRepresentation.first);
                    solver->performMatrixVectorMultiplication(dir, x, &explicitRepresentation.second, stepBound);
                    
                    // Return a hybrid check result that stores the numerical values explicitly.
                    return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), model.getReachableStates() && !maybeStates, psiStates.toAdd(), maybeStates, odd, x));
                } else {
                    return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), psiStates.toAdd()));
                }
            }
            
            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeInstantaneousRewards(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, RewardModelType const& rewardModel, uint_fast64_t stepBound, storm::utility::solver::MinMaxLinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
                // Only compute the result if the model has at least one reward this->getModel().
                STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
                
                // Create the ODD for the translation between symbolic and explicit storage.
                storm::dd::Odd<DdType> odd(model.getReachableStates());
                
                // Translate the symbolic matrix to its explicit representations.
                storm::storage::SparseMatrix<ValueType> explicitMatrix = transitionMatrix.toMatrix(model.getNondeterminismVariables(), odd, odd);
                
                // Create the solution vector (and initialize it to the state rewards of the model).
                std::vector<ValueType> x = rewardModel.getStateRewardVector().template toVector<ValueType>(model.getNondeterminismVariables(), odd, explicitMatrix.getRowGroupIndices());
                
                // Perform the matrix-vector multiplication.
                std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(explicitMatrix);
                solver->performMatrixVectorMultiplication(dir, x, nullptr, stepBound);
                
                // Return a hybrid check result that stores the numerical values explicitly.
                return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), model.getManager().getBddZero(), model.getManager().getAddZero(), model.getReachableStates(), odd, x));
            }

            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeCumulativeRewards(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, RewardModelType const& rewardModel, uint_fast64_t stepBound, storm::utility::solver::MinMaxLinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
                // Only compute the result if the model has at least one reward this->getModel().
                STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
                
                // Compute the reward vector to add in each step based on the available reward models.
                storm::dd::Add<DdType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix, model.getColumnVariables());
                
                // Create the ODD for the translation between symbolic and explicit storage.
                storm::dd::Odd<DdType> odd(model.getReachableStates());
                
                // Create the solution vector.
                std::vector<ValueType> x(model.getNumberOfStates(), storm::utility::zero<ValueType>());
                
                // Translate the symbolic matrix/vector to their explicit representations.
                storm::storage::SparseMatrix<ValueType> explicitMatrix = transitionMatrix.toMatrix(model.getNondeterminismVariables(), odd, odd);
                std::vector<ValueType> b = totalRewardVector.template toVector<ValueType>(model.getNondeterminismVariables(), odd, explicitMatrix.getRowGroupIndices());
                
                // Perform the matrix-vector multiplication.
                std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(explicitMatrix);
                solver->performMatrixVectorMultiplication(dir, x, &b, stepBound);
                
                // Return a hybrid check result that stores the numerical values explicitly.
                return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), model.getManager().getBddZero(), model.getManager().getAddZero(), model.getReachableStates(), odd, x));
            }

            template<storm::dd::DdType DdType, typename ValueType>
            std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType> const& model, storm::dd::Add<DdType> const& transitionMatrix, RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates, bool qualitative, storm::utility::solver::MinMaxLinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
                
                // Only compute the result if there is at least one reward model.
                STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
                
                // Determine which states have a reward of infinity by definition.
                storm::dd::Bdd<DdType> infinityStates;
                storm::dd::Bdd<DdType> transitionMatrixBdd = transitionMatrix.notZero();
                if (dir == OptimizationDirection::Minimize) {
                    infinityStates = storm::utility::graph::performProb1E(model, transitionMatrixBdd, model.getReachableStates(), targetStates, storm::utility::graph::performProbGreater0E(model, transitionMatrixBdd, model.getReachableStates(), targetStates));
                } else {
                    infinityStates = storm::utility::graph::performProb1A(model, transitionMatrixBdd, model.getReachableStates(), targetStates, storm::utility::graph::performProbGreater0A(model, transitionMatrixBdd, model.getReachableStates(), targetStates));
                }
                infinityStates = !infinityStates && model.getReachableStates();
                storm::dd::Bdd<DdType> maybeStates = (!targetStates && !infinityStates) && model.getReachableStates();
                STORM_LOG_INFO("Found " << infinityStates.getNonZeroCount() << " 'infinity' states.");
                STORM_LOG_INFO("Found " << targetStates.getNonZeroCount() << " 'target' states.");
                STORM_LOG_INFO("Found " << maybeStates.getNonZeroCount() << " 'maybe' states.");
                
                // Check whether we need to compute exact rewards for some states.
                if (qualitative) {
                    // Set the values for all maybe-states to 1 to indicate that their reward values
                    // are neither 0 nor infinity.
                    return std::unique_ptr<CheckResult>(new SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), infinityStates.toAdd() * model.getManager().getConstant(storm::utility::infinity<ValueType>()) + maybeStates.toAdd() * model.getManager().getConstant(storm::utility::one<ValueType>())));
                } else {
                    // If there are maybe states, we need to solve an equation system.
                    if (!maybeStates.isZero()) {
                        // Create the ODD for the translation between symbolic and explicit storage.
                        storm::dd::Odd<DdType> odd(maybeStates);
                        
                        // Create the matrix and the vector for the equation system.
                        storm::dd::Add<DdType> maybeStatesAdd = maybeStates.toAdd();
                        
                        // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
                        // non-maybe states in the matrix.
                        storm::dd::Add<DdType> submatrix = transitionMatrix * maybeStatesAdd;
                        
                        // Then compute the state reward vector to use in the computation.
                        storm::dd::Add<DdType> subvector = rewardModel.getTotalRewardVector(maybeStatesAdd, submatrix, model.getColumnVariables());
                        
                        // Before cutting the non-maybe columns, we need to compute the sizes of the row groups.
                        storm::dd::Add<DdType> stateActionAdd = (submatrix.notZero().existsAbstract(model.getColumnVariables()) || subvector.notZero()).toAdd();
                        std::vector<uint_fast64_t> rowGroupSizes = stateActionAdd.sumAbstract(model.getNondeterminismVariables()).template toVector<uint_fast64_t>(odd);
                        
                        // Finally cut away all columns targeting non-maybe states.
                        submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());
                        
                        // Create the solution vector.
                        std::vector<ValueType> x(maybeStates.getNonZeroCount(), storm::utility::zero<ValueType>());
                        
                        // Translate the symbolic matrix/vector to their explicit representations.
                        std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation = submatrix.toMatrixVector(subvector, std::move(rowGroupSizes), model.getNondeterminismVariables(), odd, odd);
                        
                        // Now solve the resulting equation system.
                        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(explicitRepresentation.first);
                        solver->solveEquationSystem(dir, x, explicitRepresentation.second);
                        
                        // Return a hybrid check result that stores the numerical values explicitly.
                        return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), model.getReachableStates() && !maybeStates, infinityStates.toAdd() * model.getManager().getConstant(storm::utility::infinity<ValueType>()), maybeStates, odd, x));
                    } else {
                        return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), infinityStates.toAdd() * model.getManager().getConstant(storm::utility::infinity<ValueType>())));
                    }
                }
            }
            
            template class HybridMdpPrctlHelper<storm::dd::DdType::CUDD, double>;
        }
    }
}