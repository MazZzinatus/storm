#include "SolveGoal.h"

#include  <memory>

#include "storm/utility/solver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"

namespace storm {
    namespace storage {
        template <typename ValueType> class SparseMatrix;
    }
    
    namespace solver {
        
        template<typename ValueType>
        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> configureMinMaxLinearEquationSolver(BoundedGoal<ValueType> const& goal, storm::solver::MinMaxLinearEquationSolverFactory<ValueType> const& factory, storm::storage::SparseMatrix<ValueType> const& matrix) {
            std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> p = factory.create(matrix);
            p->setOptimizationDirection(goal.direction());
            p->setTerminationCondition(std::make_unique<TerminateIfFilteredExtremumExceedsThreshold<ValueType>>(goal.relevantValues(), goal.boundIsStrict(), goal.thresholdValue(), goal.minimize()));
            return p;
        }
        
        template<typename ValueType> 
        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> configureMinMaxLinearEquationSolver(SolveGoal const& goal, storm::solver::MinMaxLinearEquationSolverFactory<ValueType> const& factory, storm::storage::SparseMatrix<ValueType> const& matrix) {
            if (goal.isBounded()) {
                return configureMinMaxLinearEquationSolver(static_cast<BoundedGoal<ValueType> const&>(goal), factory, matrix);
            }  
            std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = factory.create(matrix);
            solver->setOptimizationDirection(goal.direction());
            return solver;
        }
        
        template<typename ValueType>
        std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> configureLinearEquationSolver(BoundedGoal<ValueType> const& goal, storm::solver::LinearEquationSolverFactory<ValueType> const& factory, storm::storage::SparseMatrix<ValueType> const& matrix) {
            std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver = factory.create(matrix);
            solver->setTerminationCondition(std::make_unique<TerminateIfFilteredExtremumExceedsThreshold<double>>(goal.relevantValues(), goal.thresholdValue(), goal.boundIsStrict(), goal.minimize()));
            return solver;
        }
        
        template<typename ValueType>
        std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> configureLinearEquationSolver(SolveGoal const& goal, storm::solver::LinearEquationSolverFactory<ValueType> const& factory, storm::storage::SparseMatrix<ValueType> const& matrix) {
            if (goal.isBounded()) {
                return configureLinearEquationSolver(static_cast<BoundedGoal<ValueType> const&>(goal), factory, matrix);
            }
            return factory.create(matrix);
        }
    
        template std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<double>> configureMinMaxLinearEquationSolver(BoundedGoal<double> const& goal, storm::solver::MinMaxLinearEquationSolverFactory<double> const& factory, storm::storage::SparseMatrix<double> const& matrix);
        template std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<double>> configureMinMaxLinearEquationSolver(SolveGoal const& goal, storm::solver::MinMaxLinearEquationSolverFactory<double> const& factory, storm::storage::SparseMatrix<double> const&  matrix);
        template std::unique_ptr<storm::solver::LinearEquationSolver<double>> configureLinearEquationSolver(BoundedGoal<double> const& goal, storm::solver::LinearEquationSolverFactory<double> const& factory, storm::storage::SparseMatrix<double> const&  matrix);
        template std::unique_ptr<storm::solver::LinearEquationSolver<double>> configureLinearEquationSolver(SolveGoal const& goal, storm::solver::LinearEquationSolverFactory<double> const& factory, storm::storage::SparseMatrix<double> const&  matrix);

#ifdef STORM_HAVE_CARL
        template std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<storm::RationalNumber>> configureMinMaxLinearEquationSolver(BoundedGoal<storm::RationalNumber> const& goal, storm::solver::MinMaxLinearEquationSolverFactory<storm::RationalNumber> const& factory, storm::storage::SparseMatrix<storm::RationalNumber> const& matrix);
        template std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<storm::RationalNumber>> configureMinMaxLinearEquationSolver(SolveGoal const& goal, storm::solver::MinMaxLinearEquationSolverFactory<storm::RationalNumber> const& factory, storm::storage::SparseMatrix<storm::RationalNumber> const&  matrix);
#endif
    }
}
