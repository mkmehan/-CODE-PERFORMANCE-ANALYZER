#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>
#include <utility>

namespace{

std::vector<int>data;


int partition(
    std::vector<int>&data,
    int left,
    int right
){

    int pivot=
        data[left+(right-left)/2];

    int i=left;
    int j=right;

    while(i<=j){

        while(i<=right && data[i]<pivot){
            i++;
        }

        while(j>=left && data[j]>pivot){
            j--;
        }

        if(i<=j){

            std::swap(
                data[i],
                data[j]
            );

            i++;
            j--;
        }
    }

    return i;
}


void quick_sort(
    std::vector<int>&data,
    int left,
    int right
){

    if(left>=right){
        return;
    }

    int index=
        partition(
            data,
            left,
            right
        );

    if(left<index-1){

        quick_sort(
            data,
            left,
            index-1
        );
    }

    if(index<right){

        quick_sort(
            data,
            index,
            right
        );
    }
}


void setup_quick_sort(){

    data=get_sorting_input();
}


void run_quick_sort(){

    if(data.empty()){
        return;
    }

    quick_sort(
        data,
        0,
        static_cast<int>(data.size())-1
    );
}


struct QuickSortRegistration{

    QuickSortRegistration(){

        get_benchmark_runner().add(

            "quick",

            "Quick Sort",

            setup_quick_sort,

            run_quick_sort
        );
    }
};


QuickSortRegistration registration;

}