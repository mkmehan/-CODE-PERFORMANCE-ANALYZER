#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>
#include <algorithm>
#include <functional>

namespace{

std::vector<int>data;


void setup_bubble_sort(size_t n){

    data=get_sorting_input(n);
}


void run_bubble_sort(size_t){

    int n=static_cast<int>(
        data.size()
    );


    for(int i=0;i<n-1;i++){

        bool swapped=false;


        for(int j=0;j<n-i-1;j++){

            if(data[j]>data[j+1]){

                std::swap(
                    data[j],
                    data[j+1]
                );

                swapped=true;
            }
        }


        if(!swapped){

            break;
        }
    }
}

}


void register_bubble_sort(
    BenchmarkRunner&runner
){

    runner.add(

        "bubble",

        "Bubble Sort",

        setup_bubble_sort,

        run_bubble_sort
    );
}