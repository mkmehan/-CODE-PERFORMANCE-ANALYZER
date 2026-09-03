#include "../BenchmarkRunner.h"


void register_sum(
    BenchmarkRunner&runner
){

    runner.add(

        "sum",

        "Sum",

        [](size_t){
        },

        [](size_t n){

            volatile long long sum=0;

            for(size_t i=0;i<n;i++){

                sum+=i;
            }
        }
    );
}
