#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>


namespace{

struct SelectionSortRegistration{

    std::vector<int>data;


    SelectionSortRegistration(){

        get_benchmark_runner().add(

            "selection",

            "Selection Sort",

            [this](){

                data=get_sorting_input();
            },

            [this](){

                int n=data.size();


                for(int i=0;i<n-1;i++){

                    int minimum=i;


                    for(int j=i+1;j<n;j++){

                        if(
                            data[j]
                            <
                            data[minimum]
                        ){

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
        );
    }
};


SelectionSortRegistration registration;

}