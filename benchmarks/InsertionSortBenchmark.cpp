#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>
#include <algorithm>

namespace{

std::vector<int>data;


void setup_insertion_sort(size_t n){

    data=get_sorting_input(n);
}


void run_insertion_sort(size_t){

    int n=static_cast<int>(
        data.size()
    );


    for(int i=1;i<n;i++){

        int key=data[i];

        int j=i-1;


        while(
            j>=0 &&
            data[j]>key
        ){

            data[j+1]=data[j];

            j--;
        }


        data[j+1]=key;
    }
}

}


void register_insertion_sort(
    BenchmarkRunner&runner
){

    runner.add(

        "insertion",

        "Insertion Sort",

        setup_insertion_sort,

        run_insertion_sort
    );
}