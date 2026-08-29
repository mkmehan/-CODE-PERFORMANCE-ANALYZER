
#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>


namespace{

struct InsertionSortRegistration{

    std::vector<int>data;


    InsertionSortRegistration(){

        get_benchmark_runner().add(

            "insertion",

            "Insertion Sort",

            [this](){

                data=get_sorting_input();
            },

            [this](){

                int n=data.size();


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
        );
    }
};


InsertionSortRegistration registration;

}