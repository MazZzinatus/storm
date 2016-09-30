#include <string>

#include "src/storage/ppg/ProgramGraph.h"
#include "src/storage/jani/Model.h"
#include "src/storage/jani/Location.h"
#include "src/storage/jani/EdgeDestination.h"
#include "src/storage/IntegerInterval.h"
#include "src/exceptions/NotSupportedException.h"

#include "src/utility/macros.h"

namespace storm {
    namespace builder {
        
        enum class JaniProgramGraphVariableDomainMethod  {
            Unrestricted, IntervalPropagation
        };
        
        struct JaniProgramGraphBuilderSetting {
            JaniProgramGraphVariableDomainMethod variableDomainMethod = JaniProgramGraphVariableDomainMethod::Unrestricted;
        };
        
        class JaniProgramGraphBuilder {
        public:
            static unsigned janiVersion;
            
            JaniProgramGraphBuilder(storm::ppg::ProgramGraph const& pg) : programGraph(pg) {
                rewards = programGraph.rewardVariables();
                constants = programGraph.constants();
                auto boundedVars = programGraph.constantAssigned();
                for(auto const& v : boundedVars) {
                    variableRestrictions.emplace(v, programGraph.supportForConstAssignedVariable(v));
                }
            }
            
            virtual ~JaniProgramGraphBuilder() {
                for (auto& var : variables ) {
                    delete var.second;
                }
            }
            
            //void addVariableRestriction(storm::expressions::Variable const& var, storm::IntegerInterval const& interval ) {
                
            //}
            
        
            void restrictAllVariables(int64_t from, int64_t to) {
                for (auto const& v : programGraph.getVariables()) {
                    if(isConstant(v.first)) {
                        continue;
                    }
                    if(variableRestrictions.count(v.first) > 0) {
                        continue; // Currently we ignore user bounds if we have bounded integers;
                    }
                    if(v.second.hasIntegerType() ) {
                        userVariableRestrictions.emplace(v.first, storm::storage::IntegerInterval(from, to));
                    }
                }
            }
            
            storm::jani::Model* build(std::string const& name = "program_graph") {
                expManager = programGraph.getExpressionManager();
                storm::jani::Model* model = new storm::jani::Model(name, storm::jani::ModelType::MDP, janiVersion, expManager);
                storm::jani::Automaton mainAutomaton("main");
                addProcedureVariables(*model, mainAutomaton);
                janiLocId = addProcedureLocations(mainAutomaton);
                addVariableOoBLocations(mainAutomaton);
                addEdges(mainAutomaton);
                model->addAutomaton(mainAutomaton);
                model->setStandardSystemComposition();
                return model;
            }
            
        private:
            std::string janiLocationName(storm::ppg::ProgramLocationIdentifier i) {
                return "l" + std::to_string(i);
            }
            
            std::string janiVariableOutOfBoundsLocationName(storm::ppg::ProgramVariableIdentifier i) {
                return "oob-" + programGraph.getVariableName(i);
            }
            
            storm::jani::OrderedAssignments buildOrderedAssignments(storm::jani::Automaton& automaton, storm::ppg::DeterministicProgramAction const& act) ;
            void addEdges(storm::jani::Automaton& automaton);
            std::vector<storm::jani::EdgeDestination> buildDestinations(storm::jani::Automaton& automaton, storm::ppg::ProgramEdge const& edge );
            /**
             * Helper for probabilistic assignments
             */
            std::vector<storm::jani::EdgeDestination> buildProbabilisticDestinations(storm::jani::Automaton& automaton, storm::ppg::ProgramEdge const& edge );
            
            std::pair<std::vector<storm::jani::Edge>, storm::expressions::Expression> addVariableChecks(storm::ppg::ProgramEdge const& edge);
            
            bool isUserRestrictedVariable(storm::ppg::ProgramVariableIdentifier i) const {
                return userVariableRestrictions.count(i) == 1 && !isRewardVariable(i);
            }
            
            bool isRestrictedVariable(storm::ppg::ProgramVariableIdentifier i) const {
                // Might be different from user restricted in near future.
                return (variableRestrictions.count(i) == 1 && !isRewardVariable(i)) || isUserRestrictedVariable(i);
            }
            
            storm::storage::IntegerInterval const& variableBounds(storm::ppg::ProgramVariableIdentifier i) const {
                assert(isRestrictedVariable(i));
                if (userVariableRestrictions.count(i) == 1) {
                    return userVariableRestrictions.at(i);
                } else {
                    return variableRestrictions.at(i);
                }
                
            }
            
            bool isRewardVariable(storm::ppg::ProgramVariableIdentifier i) const {
                return std::find(rewards.begin(), rewards.end(), i) != rewards.end();
            }
            
            bool isConstant(storm::ppg::ProgramVariableIdentifier i) const {
                return std::find(constants.begin(), constants.end(), i) != constants.end();
            }
            
            void addProcedureVariables(storm::jani::Model& model, storm::jani::Automaton& automaton) {
                for (auto const& v : programGraph.getVariables()) {
                    if (isConstant(v.first)) {
                        storm::jani::Constant constant(v.second.getName(), v.second, programGraph.getInitialValue(v.first));
                        model.addConstant(constant);
                    } else if (v.second.hasBooleanType()) {
                        storm::jani::BooleanVariable* janiVar = new storm::jani::BooleanVariable(v.second.getName(), v.second, programGraph.getInitialValue(v.first), false);
                        automaton.addVariable(*janiVar);
                        variables.emplace(v.first, janiVar);
                    } else if (isRestrictedVariable(v.first) && !isRewardVariable(v.first)) {
                        storm::storage::IntegerInterval const& bounds = variableBounds(v.first);
                        if (bounds.hasLeftBound()) {
                            if (bounds.hasRightBound()) {
                                storm::jani::BoundedIntegerVariable* janiVar = new storm::jani::BoundedIntegerVariable (v.second.getName(), v.second, programGraph.getInitialValue(v.first), false, expManager->integer(bounds.getLeftBound().get()), expManager->integer(bounds.getRightBound().get()));
                                variables.emplace(v.first, janiVar);
                                automaton.addVariable(*janiVar);
                            } else {
                                // Not yet supported.
                                assert(false);
                            }
                        } else {
                            // Not yet supported.
                            assert(false);
                        }
                    } else {
                        storm::jani::UnboundedIntegerVariable* janiVar = new storm::jani::UnboundedIntegerVariable(v.second.getName(), v.second, programGraph.getInitialValue(v.first), isRewardVariable(v.first));
                        if(isRewardVariable(v.first)) {
                            model.addVariable(*janiVar);
                        } else {
                            automaton.addVariable(*janiVar);
                        }
                        variables.emplace(v.first, janiVar);
                        
                    }
                }
            }
            
            std::map<storm::ppg::ProgramLocationIdentifier, uint64_t> addProcedureLocations(storm::jani::Automaton& automaton) {
                std::map<storm::ppg::ProgramLocationIdentifier, uint64_t> result;
                for (auto it = programGraph.locationBegin(); it != programGraph.locationEnd(); ++it) {
                    storm::jani::Location janiLoc(janiLocationName(it->second.id()));
                    result[it->second.id()] = automaton.addLocation(janiLoc);
                    if (it->second.isInitial()) {
                        automaton.addInitialLocation(result[it->second.id()]);
                    }
                }
                return result;
            }
            
            void addVariableOoBLocations(storm::jani::Automaton& automaton) {
                for(auto const& restr : userVariableRestrictions) {
                    if(!isRewardVariable(restr.first)) {
                        storm::jani::Location janiLoc(janiVariableOutOfBoundsLocationName(restr.first));
                        uint64_t locId = automaton.addLocation(janiLoc);
                        varOutOfBoundsLocations[restr.first] = locId;
                    }
                    
                }
            }
            /// Transient variables
            std::vector<storm::ppg::ProgramVariableIdentifier> rewards;
            /// Variables that are constants
            std::vector<storm::ppg::ProgramVariableIdentifier> constants;
            /// Restrictions on variables (automatic)
            std::map<uint64_t, storm::storage::IntegerInterval> variableRestrictions;
            /// Restrictions on variables (provided by users)
            std::map<uint64_t, storm::storage::IntegerInterval> userVariableRestrictions;
            
            /// Locations for variables that would have gone ot o
            std::map<uint64_t, uint64_t> varOutOfBoundsLocations;
            std::map<storm::ppg::ProgramLocationIdentifier, uint64_t> janiLocId;
            std::map<storm::ppg::ProgramVariableIdentifier, storm::jani::Variable*> variables;
            
            /// The expression manager
            std::shared_ptr<storm::expressions::ExpressionManager> expManager;
            /// The program graph to be translated
            storm::ppg::ProgramGraph const& programGraph;
            
            
        };
    }
}