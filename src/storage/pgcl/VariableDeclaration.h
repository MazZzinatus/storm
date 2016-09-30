#pragma once

#include "src/storage/expressions/Expression.h"
#include "src/storage/expressions/Variable.h"

namespace storm {
    namespace pgcl {
        class VariableDeclaration {
        public:

            VariableDeclaration(storm::expressions::Variable const& var, storm::expressions::Expression const& exp)
            : variable(var), initialValue(exp)
            {
                // Not implemented.
            }
            
            storm::expressions::Variable const& getVariable() const {
                return variable;
            }
            
            storm::expressions::Expression const& getInitialValueExpression() const {
                return initialValue;
            }
            
        private:
            storm::expressions::Variable variable;
            storm::expressions::Expression initialValue;
            
        };
    }
}





