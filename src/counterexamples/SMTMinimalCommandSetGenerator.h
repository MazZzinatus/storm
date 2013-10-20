/*
 * SMTMinimalCommandSetGenerator.h
 *
 *  Created on: 01.10.2013
 *      Author: Christian Dehnert
 */

#ifndef STORM_COUNTEREXAMPLES_SMTMINIMALCOMMANDSETGENERATOR_MDP_H_
#define STORM_COUNTEREXAMPLES_SMTMINIMALCOMMANDSETGENERATOR_MDP_H_

#include <queue>
#include <chrono>

// To detect whether the usage of Z3 is possible, this include is neccessary.
#include "storm-config.h"

// If we have Z3 available, we have to include the C++ header.
#ifdef STORM_HAVE_Z3
#include "z3++.h"
#include "src/adapters/Z3ExpressionAdapter.h"
#endif

#include "src/adapters/ExplicitModelAdapter.h"
#include "src/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "src/solver/GmmxxNondeterministicLinearEquationSolver.h"

#include "src/utility/counterexamples.h"
#include "src/utility/IRUtility.h"

namespace storm {
    namespace counterexamples {
        
        /*!
         * This class provides functionality to generate a minimal counterexample to a probabilistic reachability
         * property in terms of used labels.
         */
        template <class T>
        class SMTMinimalCommandSetGenerator {
#ifdef STORM_HAVE_Z3
        private:
            struct RelevancyInformation {
                // The set of relevant states in the model.
                storm::storage::BitVector relevantStates;
                
                // The set of relevant labels.
                std::set<uint_fast64_t> relevantLabels;
                
                // A set of labels that is definitely known to be taken in the final solution.
                std::set<uint_fast64_t> knownLabels;
                
                // A list of relevant choices for each relevant state.
                std::map<uint_fast64_t, std::list<uint_fast64_t>> relevantChoicesForRelevantStates;
            };
            
            struct VariableInformation {
                // The variables associated with the relevant labels.
                std::vector<z3::expr> labelVariables;
                
                // A mapping from relevant labels to their indices in the variable vector.
                std::map<uint_fast64_t, uint_fast64_t> labelToIndexMap;

                // A set of original auxiliary variables needed for the Fu-Malik procedure.
                std::vector<z3::expr> originalAuxiliaryVariables;
                
                // A set of auxiliary variables that may be modified by the MaxSAT procedure.
                std::vector<z3::expr> auxiliaryVariables;
                
                // A vector of variables that can be used to constrain the number of variables that are set to true.
                std::vector<z3::expr> adderVariables;
            };
            
            /*!
             * Computes the set of relevant labels in the model. Relevant labels are choice labels such that there exists
             * a scheduler that satisfies phi until psi with a nonzero probability.
             *
             * @param labeledMdp The MDP to search for relevant labels.
             * @param phiStates A bit vector representing all states that satisfy phi.
             * @param psiStates A bit vector representing all states that satisfy psi.
             * @return A structure containing the relevant labels as well as states.
             */
            static RelevancyInformation determineRelevantStatesAndLabels(storm::models::Mdp<T> const& labeledMdp, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates) {
                // Create result.
                RelevancyInformation relevancyInformation;
                
                // Compute all relevant states, i.e. states for which there exists a scheduler that has a non-zero
                // probabilitiy of satisfying phi until psi.
                storm::storage::SparseMatrix<bool> backwardTransitions = labeledMdp.getBackwardTransitions();
                relevancyInformation.relevantStates = storm::utility::graph::performProbGreater0E(labeledMdp, backwardTransitions, phiStates, psiStates);
                relevancyInformation.relevantStates &= ~psiStates;

                LOG4CPLUS_DEBUG(logger, "Found " << relevancyInformation.relevantStates.getNumberOfSetBits() << " relevant states.");
                LOG4CPLUS_DEBUG(logger, relevancyInformation.relevantStates.toString());

                // Retrieve some references for convenient access.
                storm::storage::SparseMatrix<T> const& transitionMatrix = labeledMdp.getTransitionMatrix();
                std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = labeledMdp.getNondeterministicChoiceIndices();
                std::vector<std::set<uint_fast64_t>> const& choiceLabeling = labeledMdp.getChoiceLabeling();

                // Now traverse all choices of all relevant states and check whether there is a successor target state.
                // If so, the associated labels become relevant. Also, if a choice of relevant state has at least one
                // relevant successor, the choice becomes relevant.
                for (auto state : relevancyInformation.relevantStates) {
                    relevancyInformation.relevantChoicesForRelevantStates.emplace(state, std::list<uint_fast64_t>());
                    
                    for (uint_fast64_t row = nondeterministicChoiceIndices[state]; row < nondeterministicChoiceIndices[state + 1]; ++row) {
                        bool currentChoiceRelevant = false;

                        for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator successorIt = transitionMatrix.constColumnIteratorBegin(row); successorIt != transitionMatrix.constColumnIteratorEnd(row); ++successorIt) {
                            // If there is a relevant successor, we need to add the labels of the current choice.
                            if (relevancyInformation.relevantStates.get(*successorIt) || psiStates.get(*successorIt)) {
                                for (auto const& label : choiceLabeling[row]) {
                                    relevancyInformation.relevantLabels.insert(label);
                                }
                                if (!currentChoiceRelevant) {
                                    currentChoiceRelevant = true;
                                    relevancyInformation.relevantChoicesForRelevantStates[state].push_back(row);
                                }
                            }
                        }
                    }
                }
                
                
                // Compute the set of labels that are known to be taken in any case.
                relevancyInformation.knownLabels = storm::utility::counterexamples::getGuaranteedLabelSet(labeledMdp, psiStates, relevancyInformation.relevantLabels);
                if (!relevancyInformation.knownLabels.empty()) {
                    std::set<uint_fast64_t> remainingLabels;
                    std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(remainingLabels, remainingLabels.begin()));
                    relevancyInformation.relevantLabels = remainingLabels;
                }
                
//                std::vector<std::set<uint_fast64_t>> guaranteedLabels = storm::utility::counterexamples::getGuaranteedLabelSets(labeledMdp, psiStates, relevancyInformation.relevantLabels);
//                for (auto state : relevancyInformation.relevantStates) {
//                    std::cout << "state " << state << " ##########################################################" << std::endl;
//                    for (auto label : guaranteedLabels[state]) {
//                        std::cout << label << ", ";
//                    }
//                    std::cout << std::endl;
//                }
                
                std::cout << "Found " << relevancyInformation.relevantLabels.size() << " relevant and " << relevancyInformation.knownLabels.size() << " known labels.";

                LOG4CPLUS_DEBUG(logger, "Found " << relevancyInformation.relevantLabels.size() << " relevant and " << relevancyInformation.knownLabels.size() << " known labels.");
                return relevancyInformation;
            }
            
            /*!
             * Creates all necessary base expressions for the relevant labels.
             *
             * @param context The Z3 context in which to create the expressions.
             * @param relevantCommands A set of relevant labels for which to create the expressions.
             * @return A mapping from relevant labels to their corresponding expressions.
             */
            static VariableInformation createExpressionsForRelevantLabels(z3::context& context, std::set<uint_fast64_t> const& relevantLabels) {
                VariableInformation variableInformation;
                
                // Create stringstream to build expression names.
                std::stringstream variableName;
                
                for (auto label : relevantLabels) {                    
                    variableInformation.labelToIndexMap[label] = variableInformation.labelVariables.size();
                    
                    // Clear contents of the stream to construct new expression name.
                    variableName.clear();
                    variableName.str("");
                    variableName << "c" << label;
                    
                    variableInformation.labelVariables.push_back(context.bool_const(variableName.str().c_str()));
                    
                    // Clear contents of the stream to construct new expression name.
                    variableName.clear();
                    variableName.str("");
                    variableName << "h" << label;
                    
                    variableInformation.originalAuxiliaryVariables.push_back(context.bool_const(variableName.str().c_str()));
                }
                
                return variableInformation;
            }

            /*!
             * Asserts the constraints that are initially needed for the Fu-Malik procedure.
             *
             * @param program The program for which to build the constraints.
             * @param labeledMdp The MDP that results from the given program.
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver in which to assert the constraints.
             * @param variableInformation A structure with information about the variables for the labels.
             */
            static void assertFuMalikInitialConstraints(storm::ir::Program const& program, storm::models::Mdp<T> const& labeledMdp, storm::storage::BitVector const& psiStates, z3::context& context, z3::solver& solver, VariableInformation const& variableInformation, RelevancyInformation const& relevancyInformation) {
                // Assert that at least one of the labels must be taken.
                z3::expr formula = variableInformation.labelVariables.at(0);
                for (uint_fast64_t index = 1; index < variableInformation.labelVariables.size(); ++index) {
                    formula = formula || variableInformation.labelVariables.at(index);
                }
                solver.add(formula);
            }
            
            /*!
             * Asserts cuts that are derived from the explicit representation of the model and rule out a lot of
             * suboptimal solutions.
             *
             * @param labeledMdp The labeled MDP for which to compute the cuts.
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             */
            static void assertExplicitCuts(storm::models::Mdp<T> const& labeledMdp, storm::storage::BitVector const& psiStates, VariableInformation const& variableInformation, RelevancyInformation const& relevancyInformation, z3::context& context, z3::solver& solver) {
                // Walk through the MDP and
                // * identify labels enabled in initial states
                // * identify labels that can directly precede a given action
                // * identify labels that directly reach a target state
                // * identify labels that can directly follow a given action
                
                std::set<uint_fast64_t> initialLabels;
                std::map<uint_fast64_t, std::set<uint_fast64_t>> precedingLabels;
                std::set<uint_fast64_t> targetLabels;
                std::map<uint_fast64_t, std::set<uint_fast64_t>> followingLabels;
                std::map<uint_fast64_t, std::set<std::set<uint_fast64_t>>> synchronizingLabels;
                
                // Get some data from the MDP for convenient access.
                storm::storage::SparseMatrix<T> const& transitionMatrix = labeledMdp.getTransitionMatrix();
                std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = labeledMdp.getNondeterministicChoiceIndices();
                storm::storage::BitVector const& initialStates = labeledMdp.getInitialStates();
                std::vector<std::set<uint_fast64_t>> const& choiceLabeling = labeledMdp.getChoiceLabeling();
                storm::storage::SparseMatrix<bool> backwardTransitions = labeledMdp.getBackwardTransitions();
                
                for (auto currentState : relevancyInformation.relevantStates) {
                    for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(currentState)) {
                        
                        // If the choice is a synchronization choice, we need to record it.
                        if (choiceLabeling[currentChoice].size() > 1) {
                            for (auto label : choiceLabeling[currentChoice]) {
                                std::set<uint_fast64_t> synchSet(choiceLabeling[currentChoice]);
                                synchSet.erase(label);
                                synchronizingLabels[label].emplace(std::move(synchSet));
                            }
                        }
                        
                        // If the state is initial, we need to add all the choice labels to the initial label set.
                        if (initialStates.get(currentState)) {
                            for (auto label : choiceLabeling[currentChoice]) {
                                initialLabels.insert(label);
                            }
                        }
                        
                        // Iterate over successors and add relevant choices of relevant successors to the following label set.
                        bool canReachTargetState = false;
                        for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator successorIt = transitionMatrix.constColumnIteratorBegin(currentChoice), successorIte = transitionMatrix.constColumnIteratorEnd(currentChoice); successorIt != successorIte; ++successorIt) {
                            if (relevancyInformation.relevantStates.get(*successorIt)) {
                                for (auto relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(*successorIt)) {
                                    for (auto labelToAdd : choiceLabeling[relevantChoice]) {
                                        for (auto labelForWhichToAdd : choiceLabeling[currentChoice]) {
                                            followingLabels[labelForWhichToAdd].insert(labelToAdd);
                                        }
                                    }
                                }
                            } else if (psiStates.get(*successorIt)) {
                                canReachTargetState = true;
                            }
                        }
                        
                        // If the choice can reach a target state directly, we add all the labels to the target label set.
                        if (canReachTargetState) {
                            for (auto label : choiceLabeling[currentChoice]) {
                                targetLabels.insert(label);
                            }
                        }
                    }
                    
                    // Iterate over predecessors and add all choices that target the current state to the preceding
                    // label set of all labels of all relevant choices of the current state.
                    for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator predecessorIt = backwardTransitions.constColumnIteratorBegin(currentState), predecessorIte = backwardTransitions.constColumnIteratorEnd(currentState); predecessorIt != predecessorIte; ++predecessorIt) {
                        if (relevancyInformation.relevantStates.get(*predecessorIt)) {
                            for (auto predecessorChoice : relevancyInformation.relevantChoicesForRelevantStates.at(*predecessorIt)) {
                                bool choiceTargetsCurrentState = false;
                                for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator successorIt = transitionMatrix.constColumnIteratorBegin(predecessorChoice), successorIte = transitionMatrix.constColumnIteratorEnd(predecessorChoice); successorIt != successorIte; ++successorIt) {
                                    if (*successorIt == currentState) {
                                        choiceTargetsCurrentState = true;
                                    }
                                }
                                
                                if (choiceTargetsCurrentState) {
                                    for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(currentState)) {
                                        for (auto labelToAdd : choiceLabeling[predecessorChoice]) {
                                            for (auto labelForWhichToAdd : choiceLabeling[currentChoice]) {
                                                precedingLabels[labelForWhichToAdd].insert(labelToAdd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                LOG4CPLUS_DEBUG(logger, "Successfully gathered data for explicit cuts.");
                
                std::vector<z3::expr> formulae;
                
                LOG4CPLUS_DEBUG(logger, "Asserting initial label is taken.");
                // Start by asserting that we take at least one initial label. We may do so only if there is no initial
                // label that is already known. Otherwise this condition would be too strong.
                std::set<uint_fast64_t> intersection;
                std::set_intersection(initialLabels.begin(), initialLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(intersection, intersection.begin()));
                if (intersection.empty()) {
                    for (auto label : initialLabels) {
                        formulae.push_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
                    }
                    assertDisjunction(context, solver, formulae);
                    formulae.clear();
                } else {
                    // If the intersection was non-empty, we clear the set so we can re-use it later.
                    intersection.clear();
                }
                
                LOG4CPLUS_DEBUG(logger, "Asserting target label is taken.");
                // Likewise, if no target label is known, we may assert that there is at least one.
                std::set_intersection(targetLabels.begin(), targetLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(intersection, intersection.begin()));
                if (intersection.empty()) {
                    for (auto label : targetLabels) {
                        formulae.push_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
                    }
                    assertDisjunction(context, solver, formulae);
                } else {
                    // If the intersection was non-empty, we clear the set so we can re-use it later.
                    intersection.clear();
                }
                
                LOG4CPLUS_DEBUG(logger, "Asserting taken labels are followed by another label if they are not a target label.");
                // Now assert that for each non-target label, we take a following label.
                for (auto const& labelSetPair : followingLabels) {
                    formulae.clear();
                    if (targetLabels.find(labelSetPair.first) == targetLabels.end()) {
                        // Also, if there is a known label that may follow the current label, we don't need to assert
                        // anything here.
                        std::set_intersection(labelSetPair.second.begin(), labelSetPair.second.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(intersection, intersection.begin()));
                        if (intersection.empty()) {
                            formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(labelSetPair.first)));
                            for (auto followingLabel : labelSetPair.second) {
                                if (followingLabel != labelSetPair.first) {
                                    formulae.push_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(followingLabel)));
                                }
                            }
                        } else {
                            // If the intersection was non-empty, we clear the set so we can re-use it later.
                            intersection.clear();
                        }
                    }
                    if (formulae.size() > 0) {
                        assertDisjunction(context, solver, formulae);
                    }
                }
                
                LOG4CPLUS_DEBUG(logger, "Asserting synchronization cuts.");
                // Finally, assert that if we take one of the synchronizing labels, we also take one of the combinations
                // the label appears in.
                for (auto const& labelSynchronizingSetsPair : synchronizingLabels) {
                    formulae.clear();
                    if (relevancyInformation.knownLabels.find(labelSynchronizingSetsPair.first) == relevancyInformation.knownLabels.end()) {
                        formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(labelSynchronizingSetsPair.first)));
                    }
                    
                    // We need to be careful, because there may be one synchronisation set out of which all labels are
                    // known, which means we must not assert anything.
                    bool allImplicantsKnownForOneSet = false;
                    for (auto const& synchronizingSet : labelSynchronizingSetsPair.second) {
                        z3::expr currentCombination = context.bool_val(true);
                        bool allImplicantsKnownForCurrentSet = true;
                        for (auto label : synchronizingSet) {
                            if (relevancyInformation.knownLabels.find(label) == relevancyInformation.knownLabels.end()) {
                                currentCombination = currentCombination && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                            }
                        }
                        formulae.push_back(currentCombination);
                        
                        // If all implicants of the current set are known, we do not need to further build the constraint.
                        if (allImplicantsKnownForCurrentSet) {
                            allImplicantsKnownForOneSet = true;
                            break;
                        }
                    }
                    
                    if (!allImplicantsKnownForOneSet) {
                        assertDisjunction(context, solver, formulae);
                    }
                }
            }

            /*!
             * Asserts cuts that are derived from the symbolic representation of the model and rule out a lot of
             * suboptimal solutions.
             *
             * @param program The symbolic representation of the model in terms of a program.
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             */
            static void assertSymbolicCuts(storm::ir::Program const& program, storm::models::Mdp<T> const& labeledMdp, VariableInformation const& variableInformation, RelevancyInformation const& relevancyInformation, z3::context& context, z3::solver& solver) {
                std::map<uint_fast64_t, std::set<uint_fast64_t>> precedingLabels;
                std::set<uint_fast64_t> hasSynchronizingPredecessor;

                // Get some data from the MDP for convenient access.
                storm::storage::SparseMatrix<T> const& transitionMatrix = labeledMdp.getTransitionMatrix();
                std::vector<std::set<uint_fast64_t>> const& choiceLabeling = labeledMdp.getChoiceLabeling();
                storm::storage::SparseMatrix<bool> backwardTransitions = labeledMdp.getBackwardTransitions();
                
                // Compute the set of labels that may precede a given action.
                for (auto currentState : relevancyInformation.relevantStates) {
                    for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(currentState)) {
                        // Iterate over predecessors and add all choices that target the current state to the preceding
                        // label set of all labels of all relevant choices of the current state.
                        for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator predecessorIt = backwardTransitions.constColumnIteratorBegin(currentState), predecessorIte = backwardTransitions.constColumnIteratorEnd(currentState); predecessorIt != predecessorIte; ++predecessorIt) {
                            if (relevancyInformation.relevantStates.get(*predecessorIt)) {
                                for (auto predecessorChoice : relevancyInformation.relevantChoicesForRelevantStates.at(*predecessorIt)) {
                                    bool choiceTargetsCurrentState = false;
                                    for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator successorIt = transitionMatrix.constColumnIteratorBegin(predecessorChoice), successorIte = transitionMatrix.constColumnIteratorEnd(predecessorChoice); successorIt != successorIte; ++successorIt) {
                                        if (*successorIt == currentState) {
                                            choiceTargetsCurrentState = true;
                                        }
                                    }
                                    
                                    if (choiceTargetsCurrentState) {
                                        if (choiceLabeling.at(predecessorChoice).size() > 1) {
                                            for (auto label : choiceLabeling.at(currentChoice)) {
                                                hasSynchronizingPredecessor.insert(label);
                                            }
                                        }
                                        for (auto labelToAdd : choiceLabeling[predecessorChoice]) {
                                            for (auto labelForWhichToAdd : choiceLabeling[currentChoice]) {
                                                precedingLabels[labelForWhichToAdd].insert(labelToAdd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                storm::utility::ir::VariableInformation programVariableInformation = storm::utility::ir::createVariableInformation(program);
                
                // Create a context and register all variables of the program with their correct type.
                z3::context localContext;
                std::map<std::string, z3::expr> solverVariables;
                for (auto const& booleanVariable : programVariableInformation.booleanVariables) {
                    solverVariables.emplace(booleanVariable.getName(), localContext.bool_const(booleanVariable.getName().c_str()));
                }
                for (auto const& integerVariable : programVariableInformation.integerVariables) {
                    solverVariables.emplace(integerVariable.getName(), localContext.int_const(integerVariable.getName().c_str()));
                }
                
                // Now create a corresponding local solver and assert all range bounds for the integer variables.
                z3::solver localSolver(localContext);
                storm::adapters::Z3ExpressionAdapter expressionAdapter(localContext, solverVariables);
                for (auto const& integerVariable : programVariableInformation.integerVariables) {
                    z3::expr lowerBound = expressionAdapter.translateExpression(integerVariable.getLowerBound());
                    lowerBound = solverVariables.at(integerVariable.getName()) >= lowerBound;
                    localSolver.add(lowerBound);
                    
                    z3::expr upperBound = expressionAdapter.translateExpression(integerVariable.getUpperBound());
                    upperBound = solverVariables.at(integerVariable.getName()) <= upperBound;
                    localSolver.add(upperBound);
                }
                
                // Construct an expression that exactly characterizes the initial state.
                std::unique_ptr<storm::utility::ir::StateType> initialState(storm::utility::ir::getInitialState(program, programVariableInformation));
                z3::expr initialStateExpression = localContext.bool_val(true);
                for (uint_fast64_t index = 0; index < programVariableInformation.booleanVariables.size(); ++index) {
                    if (std::get<0>(*initialState).at(programVariableInformation.booleanVariableToIndexMap.at(programVariableInformation.booleanVariables[index].getName()))) {
                        initialStateExpression = initialStateExpression && solverVariables.at(programVariableInformation.booleanVariables[index].getName());
                    } else {
                        initialStateExpression = initialStateExpression && !solverVariables.at(programVariableInformation.booleanVariables[index].getName());
                    }
                }
                for (uint_fast64_t index = 0; index < programVariableInformation.integerVariables.size(); ++index) {
                    storm::ir::IntegerVariable const& variable = programVariableInformation.integerVariables[index];
                    initialStateExpression = initialStateExpression && (solverVariables.at(variable.getName()) == localContext.int_val(std::get<1>(*initialState).at(programVariableInformation.integerVariableToIndexMap.at(variable.getName()))));
                }
                
                std::map<uint_fast64_t, std::set<uint_fast64_t>> backwardImplications;
                
                // Now check for possible backward cuts.
                for (uint_fast64_t moduleIndex = 0; moduleIndex < program.getNumberOfModules(); ++moduleIndex) {
                    storm::ir::Module const& module = program.getModule(moduleIndex);
                    
                    for (uint_fast64_t commandIndex = 0; commandIndex < module.getNumberOfCommands(); ++commandIndex) {
                        storm::ir::Command const& command = module.getCommand(commandIndex);

                        // If the label of the command is not relevant, skip it entirely.
                        if (relevancyInformation.relevantLabels.find(command.getGlobalIndex()) == relevancyInformation.relevantLabels.end()) continue;
                        
                        // If the label has a synchronizing predecessor, we also need to skip it, because the following
                        // procedure can only consider predecessors in isolation.
                        if(hasSynchronizingPredecessor.find(command.getGlobalIndex()) != hasSynchronizingPredecessor.end()) continue;
                        
                        // Save the state of the solver so we can easily backtrack.
                        localSolver.push();
                        
                        // Check if the command is enabled in the initial state.
                        localSolver.add(expressionAdapter.translateExpression(command.getGuard()));
                        localSolver.add(initialStateExpression);
                        
                        z3::check_result checkResult = localSolver.check();
                        localSolver.pop();
                        localSolver.push();

                        if (checkResult == z3::unsat) {
                            localSolver.add(!expressionAdapter.translateExpression(command.getGuard()));
                            localSolver.push();
                            
                            // We need to check all commands of the all modules, because they could enable the current
                            // command via a global variable.
                            for (uint_fast64_t otherModuleIndex = 0; otherModuleIndex < program.getNumberOfModules(); ++otherModuleIndex) {
                                storm::ir::Module const& otherModule = program.getModule(otherModuleIndex);
                                
                                for (uint_fast64_t otherCommandIndex = 0; otherCommandIndex < otherModule.getNumberOfCommands(); ++otherCommandIndex) {
                                    storm::ir::Command const& otherCommand = otherModule.getCommand(otherCommandIndex);
                                    
                                    // We don't need to consider irrelevant commands and the command itself.
                                    if (relevancyInformation.relevantLabels.find(otherCommand.getGlobalIndex()) == relevancyInformation.relevantLabels.end()
                                        && relevancyInformation.knownLabels.find(otherCommand.getGlobalIndex()) == relevancyInformation.knownLabels.end()) {
                                        continue;
                                    }
                                    if (moduleIndex == otherModuleIndex && commandIndex == otherCommandIndex) continue;
                                    
                                    std::vector<z3::expr> formulae;
                                    formulae.reserve(otherCommand.getNumberOfUpdates());
                                    
                                    localSolver.push();
                                    
                                    for (uint_fast64_t updateIndex = 0; updateIndex < otherCommand.getNumberOfUpdates(); ++updateIndex) {
                                        std::unique_ptr<storm::ir::expressions::BaseExpression> weakestPrecondition = storm::utility::ir::getWeakestPrecondition(command.getGuard(), {otherCommand.getUpdate(updateIndex)});
                                        
                                        formulae.push_back(expressionAdapter.translateExpression(weakestPrecondition));
                                    }
                                    
                                    assertDisjunction(localContext, localSolver, formulae);
                                    
                                    // If the assertions were satisfiable, this means the other command could successfully
                                    // enable the current command.
                                    if (localSolver.check() == z3::sat) {
                                        backwardImplications[command.getGlobalIndex()].insert(otherCommand.getGlobalIndex());
                                    }
                                    
                                    localSolver.pop();
                                }
                            }
                            
                            // Remove the negated guard from the solver assertions.
                            localSolver.pop();
                        }
                        
                        // Restore state of solver where only the variable bounds are asserted.
                        localSolver.pop();
                    }
                }
                
                std::vector<z3::expr> formulae;
                for (auto const& labelImplicationsPair : backwardImplications) {
                    // We only need to make this an implication if the label is not already known. If it is known,
                    // we can directly assert the disjunction of the implications.
                    if (relevancyInformation.knownLabels.find(labelImplicationsPair.first) == relevancyInformation.knownLabels.end()) {
                        formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(labelImplicationsPair.first)));
                    }
                    
                    std::set<uint_fast64_t> actualImplications;
                    std::set_intersection(labelImplicationsPair.second.begin(), labelImplicationsPair.second.end(), precedingLabels.at(labelImplicationsPair.first).begin(), precedingLabels.at(labelImplicationsPair.first).end(), std::inserter(actualImplications, actualImplications.begin()));

                    // We should assert the implications if they are not already known to be true anyway.
                    std::set<uint_fast64_t> knownImplications;
                    std::set_intersection(actualImplications.begin(), actualImplications.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(knownImplications, knownImplications.begin()));

                    if (knownImplications.empty()) {
                        for (auto label : actualImplications) {
                            formulae.push_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
                        }
                    
                        assertDisjunction(context, solver, formulae);
                        formulae.clear();
                    }
                }
            }
            
            /*!
             * Asserts that the disjunction of the given formulae holds. If the content of the disjunction is empty,
             * this corresponds to asserting false.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param formulaVector A vector of expressions that shall form the disjunction.
             */
            static void assertDisjunction(z3::context& context, z3::solver& solver, std::vector<z3::expr> const& formulaVector) {
                z3::expr disjunction = context.bool_val(false);
                for (auto expr : formulaVector) {
                    disjunction = disjunction || expr;
                }
                solver.add(disjunction);
            }
            
            /*!
             * Asserts that the conjunction of the given formulae holds. If the content of the conjunction is empty,
             * this corresponds to asserting true.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param formulaVector A vector of expressions that shall form the conjunction.
             */
            static void assertConjunction(z3::context& context, z3::solver& solver, std::vector<z3::expr> const& formulaVector) {
                z3::expr conjunction = context.bool_val(true);
                for (auto expr : formulaVector) {
                    conjunction = conjunction && expr;
                }
                solver.add(conjunction);
            }
            
            /*!
             * Creates a full-adder for the two inputs and returns the resulting bit as well as the carry bit.
             *
             * @param in1 The first input to the adder.
             * @param in2 The second input to the adder.
             * @param carryIn The carry bit input to the adder.
             * @return A pair whose first component represents the carry bit and whose second component represents the
             * result bit.
             */
            static std::pair<z3::expr, z3::expr> createFullAdder(z3::expr in1, z3::expr in2, z3::expr carryIn) {
                z3::expr resultBit = (in1 && !in2 && !carryIn) || (!in1 && in2 && !carryIn) || (!in1 && !in2 && carryIn) || (in1 && in2 && carryIn);
                z3::expr carryBit = in1 && in2 || in1 && carryIn || in2 && carryIn;
                
                return std::make_pair(carryBit, resultBit);
            }
            
            /*!
             * Creates an adder for the two inputs of equal size. The resulting vector represents the different bits of
             * the sum (and is thus one bit longer than the two inputs).
             *
             * @param context The Z3 context in which to build the expressions.
             * @param in1 The first input to the adder.
             * @param in2 The second input to the adder.
             * @return A vector representing the bits of the sum of the two inputs.
             */
            static std::vector<z3::expr> createAdder(z3::context& context, std::vector<z3::expr> const& in1, std::vector<z3::expr> const& in2) {
                // Sanity check for sizes of input.
                if (in1.size() != in2.size() || in1.size() == 0) {
                    LOG4CPLUS_ERROR(logger, "Illegal input to adder (" << in1.size() << ", " << in2.size() << ").");
                    throw storm::exceptions::InvalidArgumentException() << "Illegal input to adder.";
                }
                
                // Prepare result.
                std::vector<z3::expr> result;
                result.reserve(in1.size() + 1);
                
                // Add all bits individually and pass on carry bit appropriately.
                z3::expr carryBit = context.bool_val(false);
                for (uint_fast64_t currentBit = 0; currentBit < in1.size(); ++currentBit) {
                    std::pair<z3::expr, z3::expr> localResult = createFullAdder(in1[currentBit], in2[currentBit], carryBit);
                    
                    result.push_back(localResult.second);
                    carryBit = localResult.first;
                }
                result.push_back(carryBit);
                
                return result;
            }
            
            /*!
             * Given a number of input numbers, creates a number of output numbers that corresponds to the sum of two
             * consecutive numbers of the input. If the number if input numbers is odd, the last number is simply added
             * to the output.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param in A vector or binary encoded numbers.
             * @return A vector of numbers that each correspond to the sum of two consecutive elements of the input.
             */
            static std::vector<std::vector<z3::expr>> createAdderPairs(z3::context& context, std::vector<std::vector<z3::expr>> const& in) {
                std::vector<std::vector<z3::expr>> result;
                
                result.reserve(in.size() / 2 + in.size() % 2);
                
                for (uint_fast64_t index = 0; index < in.size() / 2; ++index) {
                    result.push_back(createAdder(context, in[2 * index], in[2 * index + 1]));
                }
                
                if (in.size() % 2 != 0) {
                    result.push_back(in.back());
                    result.back().push_back(context.bool_val(false));
                }
                
                return result;
            }
            
            /*!
             * Creates a counter circuit that returns the number of literals out of the given vector that are set to true.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param literals The literals for which to create the adder circuit.
             * @return A bit vector representing the number of literals that are set to true.
             */
            static std::vector<z3::expr> createCounterCircuit(z3::context& context, std::vector<z3::expr> const& literals) {
                LOG4CPLUS_DEBUG(logger, "Creating counter circuit for " << literals.size() << " literals.");

                // Create the auxiliary vector.
                std::vector<std::vector<z3::expr>> aux;
                for (uint_fast64_t index = 0; index < literals.size(); ++index) {
                    aux.emplace_back();
                    aux.back().push_back(literals[index]);
                }
                
                while (aux.size() > 1) {
                    aux = createAdderPairs(context, aux);
                }
                
                return aux[0];
            }
            
            /*!
             * Determines whether the bit at the given index is set in the given value.
             *
             * @param value The value to test.
             * @param index The index of the bit to test.
             * @return True iff the bit at the given index is set in the given value.
             */
            static bool bitIsSet(uint64_t value, uint64_t index) {
                uint64_t mask = 1 << (index & 63);
                return (value & mask) != 0;
            }
            
            /*!
             * Asserts a constraint in the given solver that expresses that the value encoded by the given input variables
             * may at most represent the number k. The constraint is associated with a relaxation variable, that is
             * returned by this function and may be used to deactivate the constraint.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param input The variables that encode the value to restrict.
             * @param k The bound for the binary-encoded value.
             * @return The relaxation variable associated with the constraint.
             */
             static z3::expr assertLessOrEqualKRelaxed(z3::context& context, z3::solver& solver, std::vector<z3::expr> const& input, uint64_t k) {
                LOG4CPLUS_DEBUG(logger, "Asserting solution has size less or equal " << k << ".");

                z3::expr result(context);
                if (bitIsSet(k, 0)) {
                    result = context.bool_val(true);
                } else {
                    result = !input.at(0);
                }
                for (uint_fast64_t index = 1; index < input.size(); ++index) {
                    z3::expr i1(context);
                    z3::expr i2(context);
                    
                    if (bitIsSet(k, index)) {
                        i1 = !input.at(index);
                        i2 = result;
                    } else {
                        i1 = context.bool_val(false);
                        i2 = context.bool_val(false);
                    }
                    result = i1 || i2 || (!input.at(index) && result);
                }
                
                std::stringstream variableName;
                variableName << "relaxed" << k;
                z3::expr relaxingVariable = context.bool_const(variableName.str().c_str());
                result = result || relaxingVariable;
                
                solver.add(result);
                
                return relaxingVariable;
            }
            
            /*!
             * Asserts that the input vector encodes a decimal smaller or equal to one.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param input The binary encoded input number.
             */
            static void assertLessOrEqualOne(z3::context& context, z3::solver& solver, std::vector<z3::expr> input) {
                std::transform(input.begin(), input.end(), input.begin(), [](z3::expr e) -> z3::expr { return !e; });
                assertConjunction(context, solver, input);
            }
            
            /*!
             * Asserts that at most one of given literals may be true at any time.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param blockingVariables A vector of variables out of which only one may be true.
             */
            static void assertAtMostOne(z3::context& context, z3::solver& solver, std::vector<z3::expr> const& literals) {
                std::vector<z3::expr> counter = createCounterCircuit(context, literals);
                assertLessOrEqualOne(context, solver, counter);
            }
            
            /*!
             * Performs one Fu-Malik-Maxsat step.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param variableInformation A structure with information about the variables for the labels.
             * @return True iff the constraint system was satisfiable.
             */
            static bool fuMalikMaxsatStep(z3::context& context, z3::solver& solver, std::vector<z3::expr>& auxiliaryVariables, std::vector<z3::expr>& softConstraints, uint_fast64_t& nextFreeVariableIndex) {
                z3::expr_vector assumptions(context);
                for (auto const& auxiliaryVariable : auxiliaryVariables) {
                    assumptions.push_back(!auxiliaryVariable);
                }
                                
                // Check whether the assumptions are satisfiable.
                LOG4CPLUS_DEBUG(logger, "Invoking satisfiability checking.");
                z3::check_result result = solver.check(assumptions);
                LOG4CPLUS_DEBUG(logger, "Done invoking satisfiability checking.");
                
                if (result == z3::sat) {
                    return true;
                } else {
                    LOG4CPLUS_DEBUG(logger, "Computing unsat core.");
                    z3::expr_vector unsatCore = solver.unsat_core();
                    LOG4CPLUS_DEBUG(logger, "Computed unsat core.");
                    
                    std::vector<z3::expr> blockingVariables;
                    blockingVariables.reserve(unsatCore.size());
                    
                    // Create stringstream to build expression names.
                    std::stringstream variableName;
                    
                    for (uint_fast64_t softConstraintIndex = 0; softConstraintIndex < softConstraints.size(); ++softConstraintIndex) {
                        for (uint_fast64_t coreIndex = 0; coreIndex < unsatCore.size(); ++coreIndex) {
                            bool isContainedInCore = false;
                            if (softConstraints[softConstraintIndex] == unsatCore[coreIndex]) {
                                isContainedInCore = true;
                            }
                            
                            if (isContainedInCore) {
                                variableName.clear();
                                variableName.str("");
                                variableName << "b" << nextFreeVariableIndex;
                                blockingVariables.push_back(context.bool_const(variableName.str().c_str()));
                                
                                variableName.clear();
                                variableName.str("");
                                variableName << "a" << nextFreeVariableIndex;
                                ++nextFreeVariableIndex;
                                auxiliaryVariables[softConstraintIndex] = context.bool_const(variableName.str().c_str());
                                
                                softConstraints[softConstraintIndex] = softConstraints[softConstraintIndex] || blockingVariables.back();
                                
                                solver.add(softConstraints[softConstraintIndex] || auxiliaryVariables[softConstraintIndex]);
                            }
                        }
                    }
                    
                    assertAtMostOne(context, solver, blockingVariables);
                }
                
                return false;
            }
            
            /*!
             * Rules out the given command set for the given solver.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param commandSet The command set to rule out as a solution.
             * @param variableInformation A structure with information about the variables for the labels.
             */
            static void ruleOutSolution(z3::context& context, z3::solver& solver, std::set<uint_fast64_t> const& commandSet, VariableInformation const& variableInformation) {
                z3::expr blockSolutionExpression = context.bool_val(false);
                for (auto labelIndexPair : variableInformation.labelToIndexMap) {
                    if (commandSet.find(labelIndexPair.first) == commandSet.end()) {
                        blockSolutionExpression = blockSolutionExpression || variableInformation.labelVariables[labelIndexPair.second];
                    }
                }
                
                solver.add(blockSolutionExpression);
            }

            /*!
             * Determines the set of labels that was chosen by the given model.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param model The Z3 model from which to extract the information.
             * @param variableInformation A structure with information about the variables of the solver.
             */
            static std::set<uint_fast64_t> getUsedLabelSet(z3::context& context, z3::model const& model, VariableInformation const& variableInformation) {
                std::set<uint_fast64_t> result;
                for (auto const& labelIndexPair : variableInformation.labelToIndexMap) {
                    z3::expr auxValue = model.eval(variableInformation.labelVariables.at(labelIndexPair.second));
                    
                    // Check whether the auxiliary variable was set or not.
                    if (eq(auxValue, context.bool_val(true))) {
                        result.insert(labelIndexPair.first);
                    } else if (eq(auxValue, context.bool_val(false))) {
                        // Nothing to do in this case.
                    } else if (eq(auxValue, variableInformation.labelVariables.at(labelIndexPair.second))) {
                        // If the auxiliary variable is a don't care, then we don't take the corresponding command.
                    } else {
                        throw storm::exceptions::InvalidStateException() << "Could not retrieve value of boolean variable from illegal value.";
                    }
                }
                return result;
            }
            
            /*!
             * Asserts an adder structure in the given solver that counts the number of variables that are set to true
             * out of the given variables.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver for which to add the adder.
             * @param variableInformation A structure with information about the variables of the solver.
             */
            static std::vector<z3::expr> assertAdder(z3::context& context, z3::solver& solver, VariableInformation const& variableInformation) {
                std::stringstream variableName;
                std::vector<z3::expr> result;
                
                std::vector<z3::expr> adderVariables = createCounterCircuit(context, variableInformation.labelVariables);
                for (uint_fast64_t i = 0; i < adderVariables.size(); ++i) {
                    variableName.str("");
                    variableName.clear();
                    variableName << "adder" << i;
                    result.push_back(context.bool_const(variableName.str().c_str()));
                    solver.add(implies(adderVariables[i], result.back()));
                }
                
                return result;
            }
            
            /*!
             * Finds the smallest set of labels such that the constraint system of the solver is still satisfiable.
             *
             * @param context The Z3 context in which to build the expressions.
             * @param solver The solver to use for the satisfiability evaluation.
             * @param variableInformation A structure with information about the variables of the solver.
             * @param currentBound The currently known lower bound for the number of labels that need to be enabled
             * in order to satisfy the constraint system.
             * @return The smallest set of labels such that the constraint system of the solver is satisfiable.
             */
            static std::set<uint_fast64_t> findSmallestCommandSet(z3::context& context, z3::solver& solver, VariableInformation& variableInformation, uint_fast64_t& currentBound) {
                // Check if we can find a solution with the current bound.
                z3::expr assumption = !variableInformation.auxiliaryVariables.back();

                // As long as the constraints are unsatisfiable, we need to relax the last at-most-k constraint and
                // try with an increased bound.
                while (solver.check(1, &assumption) == z3::unsat) {
                    LOG4CPLUS_DEBUG(logger, "Constraint system is unsatisfiable with at most " << currentBound << " taken commands; increasing bound.");
                    solver.add(variableInformation.auxiliaryVariables.back());
                    variableInformation.auxiliaryVariables.push_back(assertLessOrEqualKRelaxed(context, solver, variableInformation.adderVariables, ++currentBound));
                    assumption = !variableInformation.auxiliaryVariables.back();
                }
                
                // At this point we know that the constraint system was satisfiable, so compute the induced label
                // set and return it.
                return getUsedLabelSet(context, solver.get_model(), variableInformation);
            }
            
            static void analyzeBadSolution(z3::context& context, z3::solver& solver, storm::models::Mdp<T> const& subMdp, storm::models::Mdp<T> const& originalMdp, storm::storage::BitVector const& psiStates, std::set<uint_fast64_t> const& commandSet, VariableInformation& variableInformation, RelevancyInformation const& relevancyInformation) {
                storm::storage::BitVector reachableStates(subMdp.getNumberOfStates());
                
                // Initialize the stack for the DFS.
                bool targetStateIsReachable = false;
                std::vector<uint_fast64_t> stack;
                stack.reserve(subMdp.getNumberOfStates());
                for (auto initialState : subMdp.getInitialStates()) {
                    stack.push_back(initialState);
                    reachableStates.set(initialState, true);
                }
                
                storm::storage::SparseMatrix<T> const& transitionMatrix = subMdp.getTransitionMatrix();
                std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = subMdp.getNondeterministicChoiceIndices();
                std::vector<std::set<uint_fast64_t>> const& subChoiceLabeling = subMdp.getChoiceLabeling();
                
                std::set<uint_fast64_t> reachableLabels;

                while (!stack.empty()) {
                    uint_fast64_t currentState = stack.back();
                    stack.pop_back();

                    for (uint_fast64_t currentChoice = nondeterministicChoiceIndices[currentState]; currentChoice < nondeterministicChoiceIndices[currentState + 1]; ++currentChoice) {
                        bool choiceTargetsRelevantState = false;
                        
                        for (typename storm::storage::SparseMatrix<T>::ConstIndexIterator successorIt = transitionMatrix.constColumnIteratorBegin(currentChoice), successorIte = transitionMatrix.constColumnIteratorEnd(currentChoice); successorIt != successorIte; ++successorIt) {
                            if (relevancyInformation.relevantStates.get(*successorIt) && currentState != *successorIt) {
                                choiceTargetsRelevantState = true;
                                if (!reachableStates.get(*successorIt)) {
                                    reachableStates.set(*successorIt, true);
                                    stack.push_back(*successorIt);
                                }
                            } else if (psiStates.get(*successorIt)) {
                                targetStateIsReachable = true;
                            }
                        }
                        
                        if (choiceTargetsRelevantState) {
                            for (auto label : subChoiceLabeling[currentChoice]) {
                                reachableLabels.insert(label);
                            }
                        }
                    }
                }
                
                LOG4CPLUS_DEBUG(logger, "Successfully performed reachability analysis.");
                
                if (targetStateIsReachable) {
                    LOG4CPLUS_ERROR(logger, "Target must be unreachable for this analysis.");
                    throw storm::exceptions::InvalidStateException() << "Target must be unreachable for this analysis.";
                }
                
                std::vector<std::set<uint_fast64_t>> const& choiceLabeling = originalMdp.getChoiceLabeling();
                std::set<uint_fast64_t> cutLabels;
                for (auto state : reachableStates) {
                    for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(state)) {
                        if (!storm::utility::set::isSubsetOf(choiceLabeling[currentChoice], commandSet)) {
                            for (auto label : choiceLabeling[currentChoice]) {
                                if (commandSet.find(label) == commandSet.end()) {
                                    cutLabels.insert(label);
                                }
                            }
                        }
                    }
                }
                
                std::vector<z3::expr> formulae;
                std::set<uint_fast64_t> unknownReachableLabels;
                std::set_difference(reachableLabels.begin(), reachableLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), std::inserter(unknownReachableLabels, unknownReachableLabels.begin()));
                for (auto label : unknownReachableLabels) {
                    formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
                }
                for (auto cutLabel : cutLabels) {
                    formulae.push_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(cutLabel)));
                }
                
                LOG4CPLUS_DEBUG(logger, "Asserting reachability implications.");
                
//                for (auto e : formulae) {
//                    std::cout << e << ", ";
//                }
//                std::cout << std::endl;
                
                assertDisjunction(context, solver, formulae);
//
//                std::cout << "formulae: " << std::endl;
//                for (auto e : formulae) {
//                    std::cout << e << ", ";
//                }
//                std::cout << std::endl;
//                
//                storm::storage::BitVector unreachableRelevantStates = ~reachableStates & relevancyInformation.relevantStates;
//                std::cout << unreachableRelevantStates.toString() << std::endl;
//                std::cout << reachableStates.toString() << std::endl;
//                std::cout << "reachable commands" << std::endl;
//                for (auto label : reachableLabels) {
//                    std::cout << label << ", ";
//                }
//                std::cout << std::endl;
//                std::cout << "cut commands" << std::endl;
//                for (auto label : cutLabels) {
//                    std::cout << label << ", ";
//                }
//                std::cout << std::endl;
                
            }
#endif
            
        public:
            static std::pair<std::set<uint_fast64_t>, uint_fast64_t > getMinimalCommandSet(storm::ir::Program program, std::string const& constantDefinitionString, storm::models::Mdp<T> const& labeledMdp, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, double probabilityThreshold, bool checkThresholdFeasible = false) {
#ifdef STORM_HAVE_Z3
                auto startTime = std::chrono::high_resolution_clock::now();
                auto endTime = std::chrono::high_resolution_clock::now();
                
                storm::utility::ir::defineUndefinedConstants(program, constantDefinitionString);

                // (0) Check whether the MDP is indeed labeled.
                if (!labeledMdp.hasChoiceLabels()) {
                    throw storm::exceptions::InvalidArgumentException() << "Minimal command set generation is impossible for unlabeled model.";
                }
                
                // (1) Check whether its possible to exceed the threshold if checkThresholdFeasible is set.
                double maximalReachabilityProbability = 0;
                storm::modelchecker::prctl::SparseMdpPrctlModelChecker<T> modelchecker(labeledMdp, new storm::solver::GmmxxNondeterministicLinearEquationSolver<T>());
                std::vector<T> result = modelchecker.checkUntil(false, phiStates, psiStates, false, nullptr);
                for (auto state : labeledMdp.getInitialStates()) {
                    maximalReachabilityProbability = std::max(maximalReachabilityProbability, result[state]);
                }
                if (maximalReachabilityProbability <= probabilityThreshold) {
                    throw storm::exceptions::InvalidArgumentException() << "Given probability threshold " << probabilityThreshold << " can not be achieved in model with maximal reachability probability of " << maximalReachabilityProbability << ".";
                }
                
                // (2) Identify all states and commands that are relevant, because only these need to be considered later.
                RelevancyInformation relevancyInformation = determineRelevantStatesAndLabels(labeledMdp, phiStates, psiStates);
                
                // (3) Create context for solver.
                z3::context context;
                
                // (4) Create the variables for the relevant commands.
                VariableInformation variableInformation = createExpressionsForRelevantLabels(context, relevancyInformation.relevantLabels);
                LOG4CPLUS_DEBUG(logger, "Created variables.");

                // (5) After all variables have been created, create a solver for that context.
                z3::solver solver(context);

                // (6) Now assert an adder whose result variables can later be used to constrain the nummber of label
                // variables that were set to true. Initially, we are looking for a solution that has no label enabled
                // and subsequently relax that.
                variableInformation.adderVariables = assertAdder(context, solver, variableInformation);
                variableInformation.auxiliaryVariables.push_back(assertLessOrEqualKRelaxed(context, solver, variableInformation.adderVariables, 0));
                
                // (7) Add constraints that cut off a lot of suboptimal solutions.
                LOG4CPLUS_DEBUG(logger, "Asserting cuts.");
                assertExplicitCuts(labeledMdp, psiStates, variableInformation, relevancyInformation, context, solver);
                LOG4CPLUS_DEBUG(logger, "Asserted explicit cuts.");
                assertSymbolicCuts(program, labeledMdp, variableInformation, relevancyInformation, context, solver);
                LOG4CPLUS_DEBUG(logger, "Asserted symbolic cuts.");
                
                // (8) Find the smallest set of commands that satisfies all constraints. If the probability of
                // satisfying phi until psi exceeds the given threshold, the set of labels is minimal and can be returned.
                // Otherwise, the current solution has to be ruled out and the next smallest solution is retrieved from
                // the solver.
                
                // Set up some variables for the iterations.
                std::set<uint_fast64_t> commandSet(relevancyInformation.relevantLabels);
                bool done = false;
                uint_fast64_t iterations = 0;
                uint_fast64_t currentBound = 0;
                maximalReachabilityProbability = 0;
                auto iterationTimer = std::chrono::high_resolution_clock::now();
                uint_fast64_t zeroProbabilityCount = 0;
                do {
                    LOG4CPLUS_DEBUG(logger, "Computing minimal command set.");
                    commandSet = findSmallestCommandSet(context, solver, variableInformation, currentBound);
                    LOG4CPLUS_DEBUG(logger, "Computed minimal command set of size " << (commandSet.size() + relevancyInformation.knownLabels.size()) << ".");
                    
                    // Restrict the given MDP to the current set of labels and compute the reachability probability.
                    commandSet.insert(relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end());
                    storm::models::Mdp<T> subMdp = labeledMdp.restrictChoiceLabels(commandSet);
                    storm::modelchecker::prctl::SparseMdpPrctlModelChecker<T> modelchecker(subMdp, new storm::solver::GmmxxNondeterministicLinearEquationSolver<T>());
                    LOG4CPLUS_DEBUG(logger, "Invoking model checker.");
                    std::vector<T> result = modelchecker.checkUntil(false, phiStates, psiStates, false, nullptr);
                    LOG4CPLUS_DEBUG(logger, "Computed model checking results.");
                    
                    // Now determine the maximal reachability probability by checking all initial states.
                    maximalReachabilityProbability = 0;
                    for (auto state : labeledMdp.getInitialStates()) {
                        maximalReachabilityProbability = std::max(maximalReachabilityProbability, result[state]);
                    }
                    
                    if (maximalReachabilityProbability <= probabilityThreshold) {
                        if (maximalReachabilityProbability == 0) {
                            ++zeroProbabilityCount;
                            
                            // If there was no target state reachable, analyze the solution and guide the solver into the
                            // right direction.
                            analyzeBadSolution(context, solver, subMdp, labeledMdp, psiStates, commandSet, variableInformation, relevancyInformation);
                        }
                        // In case we have not yet exceeded the given threshold, we have to rule out the current solution.
                        ruleOutSolution(context, solver, commandSet, variableInformation);
                    } else {
                        done = true;
                    }
                    ++iterations;
                    
                    endTime = std::chrono::high_resolution_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(endTime - iterationTimer).count() > 5) {
                        std::cout << "Checked " << iterations << " models in " << std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count() << "s (out of which " << zeroProbabilityCount << " could not reach the target states). Current command set size is " << commandSet.size() << std::endl;
                        iterationTimer = std::chrono::high_resolution_clock::now();
                    }
                } while (!done);
                
                std::cout << "Checked " << iterations << " models in total out of which " << zeroProbabilityCount << " could not reach the target states." << std::endl;
                
                // (9) Return the resulting command set after undefining the constants.
                storm::utility::ir::undefineUndefinedConstants(program);
                
                return std::make_pair(commandSet, iterations);
                
#else
                throw storm::exceptions::NotImplementedException() << "This functionality is unavailable since StoRM has been compiled without support for Z3.";
#endif
            }
            
            static void computeCounterexample(storm::ir::Program program, std::string const& constantDefinitionString, storm::models::Mdp<T> const& labeledMdp,  storm::property::prctl::AbstractPrctlFormula<double> const* formulaPtr) {
#ifdef STORM_HAVE_Z3
                std::cout << std::endl << "Generating minimal label counterexample for formula " << formulaPtr->toString() << std::endl;
                // First, we need to check whether the current formula is an Until-Formula.
                storm::property::prctl::ProbabilisticBoundOperator<double> const* probBoundFormula = dynamic_cast<storm::property::prctl::ProbabilisticBoundOperator<double> const*>(formulaPtr);
                if (probBoundFormula == nullptr) {
                    LOG4CPLUS_ERROR(logger, "Illegal formula " << probBoundFormula->toString() << " for counterexample generation.");
                    throw storm::exceptions::InvalidPropertyException() << "Illegal formula " << probBoundFormula->toString() << " for counterexample generation.";
                }
                if (probBoundFormula->getComparisonOperator() != storm::property::ComparisonType::LESS) {
                    LOG4CPLUS_ERROR(logger, "Illegal comparison operator in formula " << probBoundFormula->toString() << ". Only strict upper bounds are supported for counterexample generation.");
                    throw storm::exceptions::InvalidPropertyException() << "Illegal comparison operator in formula " << probBoundFormula->toString() << ". Only strict upper bounds are supported for counterexample generation.";
                }
                
                // Now derive the probability threshold we need to exceed as well as the phi and psi states. Simultaneously, check whether the formula is of a valid shape.
                double bound = probBoundFormula->getBound();
                storm::property::prctl::AbstractPathFormula<double> const& pathFormula = probBoundFormula->getPathFormula();
                storm::storage::BitVector phiStates;
                storm::storage::BitVector psiStates;
                storm::modelchecker::prctl::SparseMdpPrctlModelChecker<T> modelchecker(labeledMdp, new storm::solver::GmmxxNondeterministicLinearEquationSolver<T>());
                try {
                    storm::property::prctl::Until<double> const& untilFormula = dynamic_cast<storm::property::prctl::Until<double> const&>(pathFormula);
                    
                    phiStates = untilFormula.getLeft().check(modelchecker);
                    psiStates = untilFormula.getRight().check(modelchecker);
                } catch (std::bad_cast const& e) {
                    // If the nested formula was not an until formula, it remains to check whether it's an eventually formula.
                    try {
                        storm::property::prctl::Eventually<double> const& eventuallyFormula = dynamic_cast<storm::property::prctl::Eventually<double> const&>(pathFormula);
                        
                        phiStates = storm::storage::BitVector(labeledMdp.getNumberOfStates(), true);
                        psiStates = eventuallyFormula.getChild().check(modelchecker);
                    } catch (std::bad_cast const& e) {
                        // If the nested formula is neither an until nor a finally formula, we throw an exception.
                        throw storm::exceptions::InvalidPropertyException() << "Formula nested inside probability bound operator must be an until or eventually formula for counterexample generation.";
                    }
                }
                
                // Delegate the actual computation work to the function of equal name.
                auto startTime = std::chrono::high_resolution_clock::now();
                auto labelSetIterationPair = getMinimalCommandSet(program, constantDefinitionString, labeledMdp, phiStates, psiStates, bound, true);
                auto endTime = std::chrono::high_resolution_clock::now();
                std::cout << std::endl << "Computed minimal label set of size " << labelSetIterationPair.first.size() << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms (" << labelSetIterationPair.second << " models tested)." << std::endl;
                
                std::cout << "Resulting program:" << std::endl;
                storm::ir::Program restrictedProgram(program);
                restrictedProgram.restrictCommands(labelSetIterationPair.first);
                std::cout << restrictedProgram.toString() << std::endl;
                std::cout << std::endl << "-------------------------------------------" << std::endl;
                
                // FIXME: Return the DTMC that results from applying the max scheduler in the MDP restricted to the computed label set.
#else
                throw storm::exceptions::NotImplementedException() << "This functionality is unavailable since StoRM has been compiled without support for Z3.";
#endif
            }
            
        };
        
    } // namespace counterexamples
} // namespace storm

#endif /* STORM_COUNTEREXAMPLES_SMTMINIMALCOMMANDSETGENERATOR_MDP_H_ */