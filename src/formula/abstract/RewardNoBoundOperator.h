/*
 * RewardNoBoundOperator.h
 *
 *  Created on: 25.12.2012
 *      Author: Christian Dehnert
 */

#ifndef STORM_FORMULA_ABSTRACT_REWARDNOBOUNDOPERATOR_H_
#define STORM_FORMULA_ABSTRACT_REWARDNOBOUNDOPERATOR_H_

#include "AbstractFormula.h"
#include "PathNoBoundOperator.h"

namespace storm {
namespace property {
namespace abstract {

/*!
 * @brief
 * Class for an abstract formula tree with a R (reward) operator without declaration of reward values
 * as root.
 *
 * Checking a formula with this operator as root returns the reward for the reward path formula for
 * each state
 *
 * Has one formula as sub formula/tree.
 *
 * @note
 * 	This class is a hybrid of a state and path formula, and may only appear as the outermost operator.
 * 	Hence, it is seen as neither a state nor a path formula, but is directly derived from AbstractFormula.
 *
 * @note
 * 	This class does not contain a check() method like the other formula classes.
 * 	The check method should only be called by the model checker to infer the correct check function for sub
 * 	formulas. As this operator can only appear at the root, the method is not useful here.
 * 	Use the checkRewardNoBoundOperator method from the DtmcPrctlModelChecker class instead.
 *
 * The subtree is seen as part of the object and deleted with it
 * (this behavior can be prevented by setting them to NULL before deletion)
 *
 * @tparam FormulaType The type of the subformula.
 * 		  The instantiation of FormulaType should be a subclass of AbstractFormula, as the functions
 * 		  "toString" and "conforms" of the subformulas are needed.
 *
 * @see AbstractFormula
 * @see PathNoBoundOperator
 * @see RewardBoundOperator
 */
template <class T, class FormulaType>
class RewardNoBoundOperator: public PathNoBoundOperator<T, FormulaType> {

	// Throw a compiler error if FormulaType is not a subclass of AbstractFormula.
	static_assert(std::is_base_of<AbstractFormula<T>, FormulaType>::value,
				  "Instantiaton of FormulaType for storm::property::abstract::RewardNoBoundOperator<T,FormulaType> has to be a subtype of storm::property::abstract::AbstractFormula<T>");

public:
	/*!
	 * Empty constructor
	 */
	RewardNoBoundOperator() : PathNoBoundOperator<T, FormulaType>(nullptr) {
		// Intentionally left empty
	}

	/*!
	 * Constructor
	 *
	 * @param pathFormula The child node.
	 */
	RewardNoBoundOperator(FormulaType* pathFormula) : PathNoBoundOperator<T, FormulaType>(pathFormula) {
		// Intentionally left empty
	}

	/*!
	 * Constructor
	 *
	 * @param pathFormula The child node.
	 */
	RewardNoBoundOperator(FormulaType* pathFormula, bool minimumOperator) : PathNoBoundOperator<T, FormulaType>(pathFormula, minimumOperator) {
		// Intentionally left empty
	}

	/*!
	 * Destructor
	 */
	virtual ~RewardNoBoundOperator() {
		// Intentionally left empty
	}

	/*!
	 * @returns a string representation of the formula
	 */
	virtual std::string toString() const {
		std::string result = "R";
		result += PathNoBoundOperator<T, FormulaType>::toString();
		return result;
	}
};

} //namespace abstract
} //namespace property
} //namespace storm

#endif /* STORM_FORMULA_ABSTRACT_REWARDNOBOUNDOPERATOR_H_ */