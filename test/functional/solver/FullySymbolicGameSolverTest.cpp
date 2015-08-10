#include "gtest/gtest.h"
#include "storm-config.h"

#include "src/storage/dd/CuddDdManager.h"

#include "src/utility/solver.h"

TEST(FullySymbolicGameSolverTest, Solve) {
    // Create some variables.
    std::shared_ptr<storm::dd::DdManager<storm::dd::DdType::CUDD>> manager(new storm::dd::DdManager<storm::dd::DdType::CUDD>());
    std::pair<storm::expressions::Variable, storm::expressions::Variable> state = manager->addMetaVariable("x", 1, 4);
    std::pair<storm::expressions::Variable, storm::expressions::Variable> pl1 = manager->addMetaVariable("a", 0, 1);
    std::pair<storm::expressions::Variable, storm::expressions::Variable> pl2 = manager->addMetaVariable("b", 0, 1);

    storm::dd::Bdd<storm::dd::DdType::CUDD> allRows = manager->getBddZero();
    std::set<storm::expressions::Variable> rowMetaVariables({state.first});
    std::set<storm::expressions::Variable> columnMetaVariables({state.second});
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs = {state};
    std::set<storm::expressions::Variable> player1Variables({pl1.first});
    std::set<storm::expressions::Variable> player2Variables({pl2.first});

    // Construct simple game.
    storm::dd::Add<storm::dd::DdType::CUDD> matrix = manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 2).toAdd() * manager->getEncoding(pl1.first, 0).toAdd() * manager->getEncoding(pl2.first, 0).toAdd() * manager->getConstant(0.6);
    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 1).toAdd() * manager->getEncoding(pl1.first, 0).toAdd() * manager->getEncoding(pl2.first, 0).toAdd() * manager->getConstant(0.4);

    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 2).toAdd() * manager->getEncoding(pl1.first, 0).toAdd() * manager->getEncoding(pl2.first, 1).toAdd() * manager->getConstant(0.2);
    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 3).toAdd() * manager->getEncoding(pl1.first, 0).toAdd() * manager->getEncoding(pl2.first, 1).toAdd() * manager->getConstant(0.8);

    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 3).toAdd() * manager->getEncoding(pl1.first, 1).toAdd() * manager->getEncoding(pl2.first, 0).toAdd() * manager->getConstant(0.5);
    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 4).toAdd() * manager->getEncoding(pl1.first, 1).toAdd() * manager->getEncoding(pl2.first, 0).toAdd() * manager->getConstant(0.5);
    
    matrix += manager->getEncoding(state.first, 1).toAdd() * manager->getEncoding(state.second, 1).toAdd() * manager->getEncoding(pl1.first, 1).toAdd() * manager->getEncoding(pl2.first, 1).toAdd() * manager->getConstant(1);
    
    std::unique_ptr<storm::utility::solver::SymbolicGameSolverFactory<storm::dd::DdType::CUDD>> solverFactory(new storm::utility::solver::SymbolicGameSolverFactory<storm::dd::DdType::CUDD>());
    std::unique_ptr<storm::solver::SymbolicGameSolver<storm::dd::DdType::CUDD>> solver = solverFactory->create(matrix, allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs, player1Variables,player2Variables);
    
    // Create solution and target state vector.
    storm::dd::Add<storm::dd::DdType::CUDD> x = manager->getAddZero();
    storm::dd::Add<storm::dd::DdType::CUDD> b = manager->getEncoding(state.first, 2).toAdd() + manager->getEncoding(state.first, 4).toAdd();
    
    // Now solve the game with different strategies for the players.
    storm::dd::Add<storm::dd::DdType::CUDD> result = solver->solveGame(true, true, x, b);
    result *= manager->getEncoding(state.first, 1).toAdd();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0, result.getValue(), storm::settings::nativeEquationSolverSettings().getPrecision());
    
    x = manager->getAddZero();
    result = solver->solveGame(true, false, x, b);
    result *= manager->getEncoding(state.first, 1).toAdd();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.5, result.getValue(), storm::settings::nativeEquationSolverSettings().getPrecision());
    
    x = manager->getAddZero();
    result = solver->solveGame(false, true, x, b);
    result *= manager->getEncoding(state.first, 1).toAdd();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.2, result.getValue(), storm::settings::nativeEquationSolverSettings().getPrecision());

    x = manager->getAddZero();
    result = solver->solveGame(false, false, x, b);
    result *= manager->getEncoding(state.first, 1).toAdd();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.99999892625817599, result.getValue(), storm::settings::nativeEquationSolverSettings().getPrecision());
}