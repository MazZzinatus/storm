#include "storm/solver/GurobiLpSolver.h"

#include <numeric>

#include "storm/storage/expressions/LinearCoefficientVisitor.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/GurobiSettings.h"

#include "storm/utility/macros.h"
#include "storm/utility/constants.h"

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"

#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotImplementedException.h"


namespace storm {
    namespace solver {
        
#ifdef STORM_HAVE_GUROBI
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(std::string const& name, OptimizationDirection const& optDir) : LpSolver<ValueType>(optDir), env(nullptr), model(nullptr), nextVariableIndex(0) {
            // Create the environment.
            int error = GRBloadenv(&env, "");
            if (error || env == nullptr) {
				STORM_LOG_ERROR("Could not initialize Gurobi (" << GRBgeterrormsg(env) << ", error code " << error << ").");
				throw storm::exceptions::InvalidStateException() << "Could not initialize Gurobi environment (" << GRBgeterrormsg(env) << ", error code " << error << ").";
            }
            
            // Set some general properties of the environment.
            setGurobiEnvironmentProperties();
            
            // Create the model.
            error = GRBnewmodel(env, &model, name.c_str(), 0, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (error) {
				STORM_LOG_ERROR("Could not initialize Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
				throw storm::exceptions::InvalidStateException() << "Could not initialize Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").";
            }
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(std::string const& name) : GurobiLpSolver(name, OptimizationDirection::Minimize) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(OptimizationDirection const& optDir) : GurobiLpSolver("", optDir) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver() : GurobiLpSolver("", OptimizationDirection::Minimize) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::~GurobiLpSolver() {
            // Dispose of the objects allocated inside Gurobi.
            GRBfreemodel(model);
            GRBfreeenv(env);
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::setGurobiEnvironmentProperties() const {
			int error = 0;

			// Enable the following line to only print the output of Gurobi if the debug flag is set.
            toggleOutput(storm::settings::getModule<storm::settings::modules::DebugSettings>().isDebugSet() || storm::settings::getModule<storm::settings::modules::GurobiSettings>().isOutputSet());
            // Enable the following line to restrict Gurobi to one thread only.
            error = GRBsetintparam(env, "Threads", storm::settings::getModule<storm::settings::modules::GurobiSettings>().getNumberOfThreads());
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi Parameter Threads (" << GRBgeterrormsg(env) << ", error code " << error << ").");

            // Enable the following line to force Gurobi to be as precise about the binary variables as required by the given precision option.
            error = GRBsetdblparam(env, "IntFeasTol", storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance());
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi Parameter IntFeasTol (" << GRBgeterrormsg(env) << ", error code " << error << ").");
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::update() const {
            int error = GRBupdatemodel(model);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to update Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            // Since the model changed, we erase the optimality flag.
            this->currentModelHasBeenOptimized = false;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBoundedContinuousVariable(std::string const& name, ValueType lowerBound, ValueType upperBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getRationalType());
            this->addVariable(newVariable, GRB_CONTINUOUS, lowerBound, upperBound, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addLowerBoundedContinuousVariable(std::string const& name, ValueType lowerBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getRationalType());
            this->addVariable(newVariable, GRB_CONTINUOUS, lowerBound, GRB_INFINITY, objectiveFunctionCoefficient);
            return newVariable;
        }

        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUpperBoundedContinuousVariable(std::string const& name, ValueType upperBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getRationalType());
            this->addVariable(newVariable, GRB_CONTINUOUS, -GRB_INFINITY, upperBound, objectiveFunctionCoefficient);
            return newVariable;
        }

        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUnboundedContinuousVariable(std::string const& name, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getRationalType());
            this->addVariable(newVariable, GRB_CONTINUOUS, -GRB_INFINITY, GRB_INFINITY, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBoundedIntegerVariable(std::string const& name, ValueType lowerBound, ValueType upperBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getIntegerType());
            this->addVariable(newVariable, GRB_INTEGER, lowerBound, upperBound, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addLowerBoundedIntegerVariable(std::string const& name, ValueType lowerBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getIntegerType());
            this->addVariable(newVariable, GRB_INTEGER, lowerBound, GRB_INFINITY, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUpperBoundedIntegerVariable(std::string const& name, ValueType upperBound, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getIntegerType());
            this->addVariable(newVariable, GRB_INTEGER, -GRB_INFINITY, upperBound, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUnboundedIntegerVariable(std::string const& name, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getIntegerType());
            this->addVariable(newVariable, GRB_INTEGER, -GRB_INFINITY, GRB_INFINITY, objectiveFunctionCoefficient);
            return newVariable;
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBinaryVariable(std::string const& name, ValueType objectiveFunctionCoefficient) {
            storm::expressions::Variable newVariable = this->manager->declareVariable(name, this->manager->getIntegerType());
            this->addVariable(newVariable, GRB_BINARY, 0, 1, objectiveFunctionCoefficient);
            return newVariable;
        }

        template<typename ValueType>
        void GurobiLpSolver<ValueType>::addVariable(storm::expressions::Variable const& variable, char variableType, ValueType lowerBound, ValueType upperBound, ValueType objectiveFunctionCoefficient) {
            // Check for valid variable type.
            STORM_LOG_ASSERT(variableType == GRB_CONTINUOUS || variableType == GRB_INTEGER || variableType == GRB_BINARY, "Illegal type '" << variableType << "' for Gurobi variable.");
            
            // Finally, create the actual variable.
            int error = 0;
            error = GRBaddvar(model, 0, nullptr, nullptr, storm::utility::convertNumber<double>(objectiveFunctionCoefficient), storm::utility::convertNumber<double>(lowerBound), storm::utility::convertNumber<double>(upperBound), variableType, variable.getName().c_str());
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Could not create binary Gurobi variable (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            this->variableToIndexMap.emplace(variable, nextVariableIndex);
            ++nextVariableIndex;
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::addConstraint(std::string const& name, storm::expressions::Expression const& constraint) {
            STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");
            STORM_LOG_THROW(constraint.getOperator() != storm::expressions::OperatorType::NotEqual, storm::exceptions::InvalidArgumentException, "Illegal constraint uses inequality operator.");
            
            storm::expressions::LinearCoefficientVisitor::VariableCoefficients leftCoefficients = storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(0));
            storm::expressions::LinearCoefficientVisitor::VariableCoefficients rightCoefficients = storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(1));
            leftCoefficients.separateVariablesFromConstantPart(rightCoefficients);
            
            // Now we need to transform the coefficients to the vector representation.
            std::vector<int> variables;
            std::vector<double> coefficients;
            for (auto const& variableCoefficientPair : leftCoefficients) {
                auto variableIndexPair = this->variableToIndexMap.find(variableCoefficientPair.first);
                variables.push_back(variableIndexPair->second);
                coefficients.push_back(leftCoefficients.getCoefficient(variableIndexPair->first));
            }
            
            // Determine the type of the constraint and add it properly.
            int error = 0;
            switch (constraint.getOperator()) {
                case storm::expressions::OperatorType::Less:
                    error = GRBaddconstr(model, variables.size(), variables.data(), coefficients.data(), GRB_LESS_EQUAL, rightCoefficients.getConstantPart() - storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance(), name == "" ? nullptr : name.c_str());
                    break;
                case storm::expressions::OperatorType::LessOrEqual:
                    error = GRBaddconstr(model, variables.size(), variables.data(), coefficients.data(), GRB_LESS_EQUAL, rightCoefficients.getConstantPart(), name == "" ? nullptr : name.c_str());
                    break;
                case storm::expressions::OperatorType::Greater:
                    error = GRBaddconstr(model, variables.size(), variables.data(), coefficients.data(), GRB_GREATER_EQUAL, rightCoefficients.getConstantPart() + storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance(), name == "" ? nullptr : name.c_str());
                    break;
                case storm::expressions::OperatorType::GreaterOrEqual:
                    error = GRBaddconstr(model, variables.size(), variables.data(), coefficients.data(), GRB_GREATER_EQUAL, rightCoefficients.getConstantPart(), name == "" ? nullptr : name.c_str());
                    break;
                case storm::expressions::OperatorType::Equal:
                    error = GRBaddconstr(model, variables.size(), variables.data(), coefficients.data(), GRB_EQUAL, rightCoefficients.getConstantPart(), name == "" ? nullptr : name.c_str());
                    break;
                default:
                    STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
            }
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Could not assert constraint (" << GRBgeterrormsg(env) << ", error code " << error << ").");
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::optimize() const {
            // First incorporate all recent changes.
            this->update();
         
            // Set the most recently set model sense.
            int error = GRBsetintattr(model, "ModelSense", this->getOptimizationDirection() == OptimizationDirection::Minimize ? 1 : -1);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi model sense (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            // Then we actually optimize the model.
            error = GRBoptimize(model);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to optimize Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            this->currentModelHasBeenOptimized = true;
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isInfeasible() const {
            if (!this->currentModelHasBeenOptimized) {
                throw storm::exceptions::InvalidStateException() << "Illegal call to GurobiLpSolver<ValueType>::isInfeasible: model has not been optimized.";
            }

            int optimalityStatus = 0;
            
            int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            // By default, Gurobi may tell us only that the model is either infeasible or unbounded. To decide which one
            // it is, we need to perform an extra step.
            if (optimalityStatus == GRB_INF_OR_UNBD) {
                error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 0);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi parameter (" << GRBgeterrormsg(env) << ", error code " << error << ").");
                
                this->optimize();
                
                error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
                
                error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 1);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi parameter (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            }
            
            return optimalityStatus == GRB_INFEASIBLE;
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isUnbounded() const {
            if (!this->currentModelHasBeenOptimized) {
                throw storm::exceptions::InvalidStateException() << "Illegal call to GurobiLpSolver<ValueType>::isUnbounded: model has not been optimized.";
            }

            int optimalityStatus = 0;
            
            int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            // By default, Gurobi may tell us only that the model is either infeasible or unbounded. To decide which one
            // it is, we need to perform an extra step.
            if (optimalityStatus == GRB_INF_OR_UNBD) {
                error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 0);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi parameter (" << GRBgeterrormsg(env) << ", error code " << error << ").");
                
                this->optimize();

                error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");

                error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 1);
                STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi parameter (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            }
            
            return optimalityStatus == GRB_UNBOUNDED;
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isOptimal() const {
            if (!this->currentModelHasBeenOptimized) {
                return false;
            }
            int optimalityStatus = 0;
            
            int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            return optimalityStatus == GRB_OPTIMAL;
        }
        
        template<typename ValueType>
        ValueType GurobiLpSolver<ValueType>::getContinuousValue(storm::expressions::Variable const& variable) const {
            if (!this->isOptimal()) {
                STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(env) << ").");
            }
            
            auto variableIndexPair = this->variableToIndexMap.find(variable);
            STORM_LOG_THROW(variableIndexPair != this->variableToIndexMap.end(), storm::exceptions::InvalidAccessException, "Accessing value of unknown variable '" << variable.getName() << "'.");
            
            double value = 0;
            int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, variableIndexPair->second, &value);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to get Gurobi solution (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            return storm::utility::convertNumber<ValueType>(value);
        }
        
        template<typename ValueType>
        int_fast64_t GurobiLpSolver<ValueType>::getIntegerValue(storm::expressions::Variable const& variable) const {
            if (!this->isOptimal()) {
                STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(env) << ").");
            }
            
            auto variableIndexPair = this->variableToIndexMap.find(variable);
            STORM_LOG_THROW(variableIndexPair != this->variableToIndexMap.end(), storm::exceptions::InvalidAccessException, "Accessing value of unknown variable '" << variable.getName() << "'.");
            
            double value = 0;
            int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, variableIndexPair->second, &value);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to get Gurobi solution (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            STORM_LOG_THROW(std::abs(static_cast<int>(value) - value) <= storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance(), storm::exceptions::InvalidStateException, "Illegal value for integer variable in Gurobi solution (" << value << ").");
            
            return static_cast<int_fast64_t>(value);
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::getBinaryValue(storm::expressions::Variable const& variable) const {
            if (!this->isOptimal()) {
                STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(env) << ").");
            }

            auto variableIndexPair = this->variableToIndexMap.find(variable);
            STORM_LOG_THROW(variableIndexPair != this->variableToIndexMap.end(), storm::exceptions::InvalidAccessException, "Accessing value of unknown variable '" << variable.getName() << "'.");
            
            double value = 0;
            int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, variableIndexPair->second, &value);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to get Gurobi solution (" << GRBgeterrormsg(env) << ", error code " << error << ").");

            if (value > 0.5) {
                STORM_LOG_THROW(std::abs(static_cast<int>(value) - 1) <= storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance(), storm::exceptions::InvalidStateException, "Illegal value for integer variable in Gurobi solution (" << value << ").");
            } else {
                STORM_LOG_THROW(value <= storm::settings::getModule<storm::settings::modules::GurobiSettings>().getIntegerTolerance(), storm::exceptions::InvalidStateException, "Illegal value for integer variable in Gurobi solution (" << value << ").");
            }
            
            return static_cast<bool>(value);
        }
        
        template<typename ValueType>
        ValueType GurobiLpSolver<ValueType>::getObjectiveValue() const {
            if (!this->isOptimal()) {
                STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(env) << ").");
                STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(env) << ").");
            }
            
            double value = 0;
            int error = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, &value);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to get Gurobi solution (" << GRBgeterrormsg(env) << ", error code " << error << ").");
            
            return storm::utility::convertNumber<ValueType>(value);
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::writeModelToFile(std::string const& filename) const {
            int error = GRBwrite(model, filename.c_str());
            if (error) {
				STORM_LOG_ERROR("Unable to write Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ") to file.");
				throw storm::exceptions::InvalidStateException() << "Unable to write Gurobi model (" << GRBgeterrormsg(env) << ", error code " << error << ") to file.";
            }
        }

        template<typename ValueType>
        void GurobiLpSolver<ValueType>::toggleOutput(bool set) const {
            int error = GRBsetintparam(env, "OutputFlag", set);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to set Gurobi Parameter OutputFlag (" << GRBgeterrormsg(env) << ", error code " << error << ").");
        }
#else
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(std::string const&, OptimizationDirection const&) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(std::string const&) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver(OptimizationDirection const&) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::GurobiLpSolver() {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        GurobiLpSolver<ValueType>::~GurobiLpSolver() {
        
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBoundedContinuousVariable(std::string const&, ValueType, ValueType, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addLowerBoundedContinuousVariable(std::string const&, ValueType, ValueType ) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";            }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUpperBoundedContinuousVariable(std::string const&, ValueType, ValueType ) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUnboundedContinuousVariable(std::string const&, ValueType ) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBoundedIntegerVariable(std::string const&, ValueType, ValueType, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addLowerBoundedIntegerVariable(std::string const&, ValueType, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUpperBoundedIntegerVariable(std::string const&, ValueType, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addUnboundedIntegerVariable(std::string const&, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        storm::expressions::Variable GurobiLpSolver<ValueType>::addBinaryVariable(std::string const&, ValueType) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::update() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::addConstraint(std::string const&, storm::expressions::Expression const&) {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::optimize() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isInfeasible() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isUnbounded() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::isOptimal() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        ValueType GurobiLpSolver<ValueType>::getContinuousValue(storm::expressions::Variable const&) const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        int_fast64_t GurobiLpSolver<ValueType>::getIntegerValue(storm::expressions::Variable const&) const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        bool GurobiLpSolver<ValueType>::getBinaryValue(storm::expressions::Variable const&) const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        ValueType GurobiLpSolver<ValueType>::getObjectiveValue() const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }
        
        template<typename ValueType>
        void GurobiLpSolver<ValueType>::writeModelToFile(std::string const&) const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }

        template<typename ValueType>
        void GurobiLpSolver<ValueType>::toggleOutput(bool) const {
            throw storm::exceptions::NotImplementedException() << "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a version of support with Gurobi support.";
        }

#endif
        template class GurobiLpSolver<double>;
        template class GurobiLpSolver<storm::RationalNumber>;
    }
}

