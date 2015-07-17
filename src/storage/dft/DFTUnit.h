#ifndef DFTUNIT_H
#define	DFTUNIT_H

namespace storm {
    namespace storage {
        class DFT;
        
        class DFTUnit {
        private:
            DFT const& mDft;
            BitVector mMembers;
            
            
        public:
            DFTUnit(DFT const& dft, BitVector const& members);
        };
    }
}


#endif	/* DFTUNIT_H */

