#include "storm/storage/prism/Label.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
    namespace prism {
        Label::Label(std::string const& name, storm::expressions::Expression const& statePredicateExpression, std::string const& filename, uint_fast64_t lineNumber) : LocatedInformation(filename, lineNumber), name(name), statePredicateExpression(statePredicateExpression) {
            // Intentionally left empty.
        }

        std::string const& Label::getName() const {
            return this->name;
        }
        
        storm::expressions::Expression const& Label::getStatePredicateExpression() const {
            return this->statePredicateExpression;
        }
        
        Label Label::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
            return Label(this->getName(), this->getStatePredicateExpression().substitute(substitution), this->getFilename(), this->getLineNumber());
        }
        
        std::ostream& operator<<(std::ostream& stream, Label const& label) {
            stream << "label \"" << label.getName() << "\" = " << label.getStatePredicateExpression() << ";";
            return stream;
        }
    } // namespace prism
} // namespace storm
