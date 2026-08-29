#ifndef SORTING_DATA_H
#define SORTING_DATA_H

#include <vector>
#include <random>

inline const std::vector<int>&get_sorting_input(){

    static const std::vector<int>data=[](){

        constexpr int INPUT_SIZE=2000;

        std::vector<int>values(INPUT_SIZE);

        std::mt19937 generator(12345);

        std::uniform_int_distribution<int>
            distribution(0,1000000);

        for(int i=0;i<INPUT_SIZE;i++){

            values[i]=distribution(generator);
        }

        return values;

    }();

    return data;
}

#endif