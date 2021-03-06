#include "storm/utility/ConstantsComparator.h"

#include <cstdlib>
#include <cmath>
#include "storm/storage/sparse/StateType.h"

#include "storm/utility/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

namespace storm {
    namespace utility {
        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isOne(ValueType const& value) const {
            return storm::utility::isOne(value);
        }
        
        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isZero(ValueType const& value) const {
            return storm::utility::isZero(value);
        }
        
        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isEqual(ValueType const& value1, ValueType const& value2) const {
            return value1 == value2;
        }
        
        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isConstant(ValueType const& value) const {
            return storm::utility::isConstant(value);
        }

        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isInfinity(ValueType const&) const {
            return false;
        }
        
        template<typename ValueType>
        bool ConstantsComparator<ValueType>::isLess(ValueType const& value1, ValueType const& value2) const {
            return value1 < value2;
        }
        
        ConstantsComparator<float>::ConstantsComparator() : precision(static_cast<float>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision())) {
            // Intentionally left empty.
        }
        
        ConstantsComparator<float>::ConstantsComparator(float precision) : precision(precision) {
            // Intentionally left empty.
        }
        
        bool ConstantsComparator<float>::isOne(float const& value) const {
            return std::abs(value - one<float>()) <= precision;
        }
        
        bool ConstantsComparator<float>::isZero(float const& value) const {
            return std::abs(value) <= precision;
        }
        
        bool ConstantsComparator<float>::isEqual(float const& value1, float const& value2) const {
            return std::abs(value1 - value2) <= precision;
        }
        
        bool ConstantsComparator<float>::isConstant(float const&) const {
            return true;
        }
        
        bool ConstantsComparator<float>::isInfinity(float const& value) const {
            return value == storm::utility::infinity<float>();
        }
        
        bool ConstantsComparator<float>::isLess(float const& value1, float const& value2) const {
            return std::abs(value1 - value2) < precision;
        }
        
        ConstantsComparator<double>::ConstantsComparator() : precision(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision()) {
            // Intentionally left empty.
        }
        
        ConstantsComparator<double>::ConstantsComparator(double precision) : precision(precision) {
            // Intentionally left empty.
        }
        
        bool ConstantsComparator<double>::isOne(double const& value) const {
            return std::abs(value - one<double>()) <= precision;
        }
        
        bool ConstantsComparator<double>::isZero(double const& value) const {
            return std::abs(value) <= precision;
        }
        
        bool ConstantsComparator<double>::isInfinity(double const& value) const {
            return value == infinity<double>();
        }
        
        bool ConstantsComparator<double>::isEqual(double const& value1, double const& value2) const {
            return std::abs(value1 - value2) <= precision;
        }
        
        bool ConstantsComparator<double>::isConstant(double const&) const {
            return true;
        }
        
        bool ConstantsComparator<double>::isLess(double const& value1, double const& value2) const {
            return value1 < value2 - precision;
        }
        
        // Explicit instantiations.
        template class ConstantsComparator<double>;
        template class ConstantsComparator<float>;
        template class ConstantsComparator<int>;
        template class ConstantsComparator<storm::storage::sparse::state_type>;

#ifdef STORM_HAVE_CARL
#if defined(STORM_HAVE_CLN)
        template class ConstantsComparator<ClnRationalNumber>;
#endif
        
#if defined(STORM_HAVE_GMP)
        template class ConstantsComparator<GmpRationalNumber>;
#endif
        
        template class ConstantsComparator<RationalFunction>;
        template class ConstantsComparator<Polynomial>;
        template class ConstantsComparator<Interval>;
#endif
    }
}
