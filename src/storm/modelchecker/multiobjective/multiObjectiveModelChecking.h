#pragma once

#include <memory>

#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelCheckingMethod.h"
#include "storm/logic/Formulas.h"

namespace storm {
    namespace modelchecker {
        namespace multiobjective {
            
            
            template<typename SparseModelType>
            std::unique_ptr<CheckResult> performMultiObjectiveModelChecking(SparseModelType const& model, storm::logic::MultiObjectiveFormula const& formula, MultiObjectiveMethodSelection methodSelection = MultiObjectiveMethodSelection::FROMSETTINGS);
            
        }
    }
}
