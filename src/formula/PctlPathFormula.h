/*
 * PctlPathFormula.h
 *
 *  Created on: 19.10.2012
 *      Author: Thomas Heinemann
 */

#ifndef MRMC_FORMULA_PCTLPATHFORMULA_H_
#define MRMC_FORMULA_PCTLPATHFORMULA_H_

#include "PctlFormula.h"
#include "modelChecker/DtmcPrctlModelChecker.h"
#include <vector>

namespace mrmc {

namespace formula {

/*!
 * @brief
 * Abstract base class for PCTL path formulas.
 *
 * @attention This class is abstract.
 * @note Formula classes do not have copy constructors. The parameters of the constructors are usually the subtrees, so
 * 	   the syntax conflicts with copy constructors for unary operators. To produce an identical object, use the method
 * 	   clone().
 */
template <class T>
class PctlPathFormula : public PctlFormula<T> {

public:
	/*!
	 * empty destructor
	 */
	virtual ~PctlPathFormula() { }

	/*!
	 * Clones the called object.
	 *
	 * Performs a "deep copy", i.e. the subtrees of the new object are clones of the original ones
	 *
	 * @note This function is not implemented in this class.
	 * @returns a new AND-object that is identical the called object.
	 */
	virtual PctlPathFormula<T>* clone() const = 0;

	/*!
	 * Calls the model checker to check this formula.
	 * Needed to infer the correct type of formula class.
	 *
	 * @note This function should only be called in a generic check function of a model checker class. For other uses,
	 *       the methods of the model checker should be used.
	 *
	 * @note This function is not implemented in this class.
	 *
	 * @returns A vector indicating the probability that the formula holds for each state.
	 */
	virtual std::vector<T>* check(const mrmc::modelChecker::DtmcPrctlModelChecker<T>& modelChecker) const = 0;
};

} //namespace formula

} //namespace mrmc

#endif /* MRMC_FORMULA_PCTLPATHFORMULA_H_ */