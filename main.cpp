#include "BenchmarkRunner.h"

#include <iostream>
#include <string>


int main(int argc,char*argv[]){

    BenchmarkRunner&runner=
        get_benchmark_runner();


    if(argc==1){

        runner.run_all();

        return 0;
    }


    std::string argument=argv[1];


    if(argument=="--help"){

        runner.list_benchmarks();

        return 0;
    }


    if(argument=="--all"){

        runner.run_all();

        return 0;
    }


    if(argument.rfind("--",0)==0){

        std::string key=
            argument.substr(2);


        if(
            runner.run_selected(key)
        ){

            return 0;
        }


        std::cout
            <<"Unknown benchmark: "
            <<argument
            <<"\n\n";


        runner.list_benchmarks();


        return 1;
    }


    std::cout
        <<"Unknown argument: "
        <<argument
        <<"\n\n";


    runner.list_benchmarks();


    return 1;
}