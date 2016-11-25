#pragma once

#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

namespace storm {
    namespace abstraction {

        template<storm::dd::DdType Type, typename ValueType>
        struct QuantitativeResult {
            QuantitativeResult() = default;
            
            QuantitativeResult(ValueType initialStateValue, storm::dd::Add<Type, ValueType> const& values, storm::dd::Bdd<Type> const& player1Strategy, storm::dd::Bdd<Type> const& player2Strategy) : initialStateValue(initialStateValue), values(values), player1Strategy(player1Strategy), player2Strategy(player2Strategy) {
                // Intentionally left empty.
            }
            
            ValueType initialStateValue;
            storm::dd::Add<Type, ValueType> values;
            storm::dd::Bdd<Type> player1Strategy;
            storm::dd::Bdd<Type> player2Strategy;
        };
        
    }
}
