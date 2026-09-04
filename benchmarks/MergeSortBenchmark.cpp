#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <algorithm>
#include <vector>


namespace{

std::vector<int>data;
std::vector<int>temp;


void merge_sort(

    std::vector<int>&data,

    std::vector<int>&temp,

    int left,

    int right

){

    if(left>=right){

        return;
    }


    int mid=
        left+(right-left)/2;


    merge_sort(
        data,
        temp,
        left,
        mid
    );


    merge_sort(
        data,
        temp,
        mid+1,
        right
    );


    int i=left;

    int j=mid+1;

    int k=left;


    while(
        i<=mid &&
        j<=right
    ){

        if(data[i]<=data[j]){

            temp[k++]=data[i++];

        }else{

            temp[k++]=data[j++];
        }
    }


    while(i<=mid){

        temp[k++]=data[i++];
    }


    while(j<=right){

        temp[k++]=data[j++];
    }


    for(int x=left;x<=right;x++){

        data[x]=temp[x];
    }
}


void setup_merge_sort(
    size_t n,
    InputDataCase input_case
){

    data=get_sorting_input(
        n,
        input_case
    );

    temp.resize(
        data.size()
    );
}


void run_merge_sort(size_t){

    if(data.empty()){

        return;
    }


    merge_sort(

        data,

        temp,

        0,

        static_cast<int>(
            data.size()
        )-1
    );
}

}


void register_merge_sort(
    BenchmarkRunner&runner
){

    runner.add(

        "merge",

        "Merge Sort",

        setup_merge_sort,

        run_merge_sort,

        []{

            return std::is_sorted(
                data.begin(),
                data.end()
            );
        }
    );
}
