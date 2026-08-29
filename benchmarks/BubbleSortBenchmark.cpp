#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <vector>


namespace{

struct BubbleSortRegistration{

    std::vector<int>data;


    BubbleSortRegistration(){

        get_benchmark_runner().add(

            "bubble",

            "Bubble Sort",

            [this](){

                data=get_sorting_input();
            },

            [this](){

                int n=data.size();


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
        );
    }
};


BubbleSortRegistration registration;

}