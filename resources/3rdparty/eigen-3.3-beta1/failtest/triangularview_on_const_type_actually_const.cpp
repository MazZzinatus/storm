#include "../StormEigen/Core"

#ifdef STORMEIGEN_SHOULD_FAIL_TO_BUILD
#define CV_QUALIFIER const
#else
#define CV_QUALIFIER
#endif

using namespace StormEigen;

void foo(){
    MatrixXf m;
    TriangularView<CV_QUALIFIER MatrixXf,Upper>(m).coeffRef(0, 0) = 1.0f;
}

int main() {}
