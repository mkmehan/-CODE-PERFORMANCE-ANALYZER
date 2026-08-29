#include "../BenchmarkRunner.h"

namespace{

struct SumBenchmarkRegistration{

    SumBenchmarkRegistration(){

        get_benchmark_runner().add(

            "sum",

            "Sum 1,000 numbers",

            [](){

                // Setup
            },

            [](){

                volatile long long sum=0;

                for(int i=1;i<=1000;i++){

                    sum+=i;
                }

            }
        );
    }
};

SumBenchmarkRegistration registration;

}