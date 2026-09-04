#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>
#include <algorithm>


namespace{

std::vector<int>data;


void setup_selection_sort(
    size_t n,
    InputDataCase input_case
){

    data=get_sorting_input(
        n,
        input_case
    );
}


void run_selection_sort(size_t){

    int n=static_cast<int>(
        data.size()
    );


    for(int i=0;i<n-1;i++){

        int minimum=i;


        for(int j=i+1;j<n;j++){

            if(data[j]<data[minimum]){

                minimum=j;
            }
        }


        if(minimum!=i){

            std::swap(
                data[i],
                data[minimum]
            );
        }
    }
}

}


void register_selection_sort(
    BenchmarkRunner&runner
){

    runner.add(

        "selection",

        "Selection Sort",

        setup_selection_sort,

        run_selection_sort,

        []{

            return std::is_sorted(
                data.begin(),
                data.end()
            );
        }
    );
}
