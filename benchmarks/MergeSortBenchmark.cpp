#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>


namespace{


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


struct MergeSortRegistration{

    std::vector<int>data;

    std::vector<int>temp;


    MergeSortRegistration(){

        get_benchmark_runner().add(

            "merge",

            "Merge Sort",

            [this](){

                data=get_sorting_input();

                temp.resize(
                    data.size()
                );
            },

            [this](){

                merge_sort(

                    data,
                    temp,
                    0,
                    data.size()-1
                );
            }
        );
    }
};


MergeSortRegistration registration;

}