#ifndef SORTING_DATA_H
#define SORTING_DATA_H

#include <vector>
#include <random>

inline std::vector<int>get_sorting_input(
    size_t size
){

    std::vector<int>values(size);

    std::mt19937 generator(12345);

    std::uniform_int_distribution<int>
        distribution(0,1000000);


    for(size_t i=0;i<size;i++){

        values[i]=distribution(generator);
    }


    return values;
}

#endif