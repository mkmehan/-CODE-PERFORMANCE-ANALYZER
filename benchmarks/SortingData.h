#ifndef SORTING_DATA_H
#define SORTING_DATA_H

#include <random>
#include <vector>

#include "../Benchmark.h"

inline std::vector<int>get_sorting_input(
    size_t size,
    InputDataCase input_case
){

    std::vector<int>values(size);


    if(input_case==InputDataCase::Sorted){

        for(size_t i=0;i<size;i++){

            values[i]=static_cast<int>(i);
        }


        return values;
    }


    if(input_case==InputDataCase::ReverseSorted){

        for(size_t i=0;i<size;i++){

            values[i]=static_cast<int>(size-i);
        }


        return values;
    }

    std::mt19937 generator(12345);

    std::uniform_int_distribution<int>
        distribution(0,1000000);


    for(size_t i=0;i<size;i++){

        values[i]=distribution(generator);
    }


    return values;
}

#endif
