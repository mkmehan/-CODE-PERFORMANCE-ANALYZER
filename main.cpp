#include "BenchmarkRunner.h"
#include "benchmarks/RegisterBenchmarks.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>


bool parse_sizes(

    const std::string&text,

    std::vector<size_t>&sizes

){

    sizes.clear();

    std::stringstream ss(text);

    std::string part;


    while(std::getline(ss,part,',')){

        if(part.empty()){
            return false;
        }


        try{

            size_t position=0;

            unsigned long long value=
                std::stoull(
                    part,
                    &position
                );


            if(position!=part.size()){
                return false;
            }


            if(value==0){
                return false;
            }


            sizes.push_back(
                static_cast<size_t>(value)
            );
        }
        catch(...){

            return false;
        }
    }


    return !sizes.empty();
}


int main(

    int argc,

    char*argv[]

){

    BenchmarkRunner runner(
        10,
        100
    );


    register_all_benchmarks(
        runner
    );


    std::string selected_benchmark="";


    for(int i=1;i<argc;i++){

        std::string argument=
            argv[i];


        if(argument=="--help"){

            runner.list_benchmarks();

            std::cout
                <<"\nUsage:\n";

            std::cout
                <<"  analyzer.exe --all\n";

            std::cout
                <<"  analyzer.exe --all --sizes 100,500,1000,2000\n";

            std::cout
                <<"  analyzer.exe --quick --sizes 1000,2000,5000\n";

            return 0;
        }


        if(argument=="--all"){

            if(!selected_benchmark.empty()){

                std::cout
                    <<"Error: --all cannot be used "
                    <<"with a selected benchmark.\n";

                return 1;
            }

            continue;
        }


        if(argument=="--sizes"){

            if(i+1>=argc){

                std::cout
                    <<"Error: --sizes requires "
                    <<"a value.\n";

                return 1;
            }


            std::string size_text=
                argv[++i];


            std::vector<size_t>sizes;


            if(!parse_sizes(
                size_text,
                sizes
            )){

                std::cout
                    <<"Error: invalid input sizes: "
                    <<size_text
                    <<"\n";

                std::cout
                    <<"Example: "
                    <<"--sizes 100,500,1000,2000\n";

                return 1;
            }


            runner.set_input_sizes(
                sizes
            );


            continue;
        }


        if(
            argument.size()>2 &&
            argument.rfind("--",0)==0
        ){

            std::string key=
                argument.substr(2);


            if(!selected_benchmark.empty()){

                std::cout
                    <<"Error: multiple benchmarks "
                    <<"were specified.\n";

                return 1;
            }


            selected_benchmark=key;

            continue;
        }


        std::cout
            <<"Unknown argument: "
            <<argument
            <<"\n\n";


        runner.list_benchmarks();

        return 1;
    }


    if(!selected_benchmark.empty()){

        if(
            !runner.run_selected(
                selected_benchmark
            )
        ){

            std::cout
                <<"Unknown benchmark: --"
                <<selected_benchmark
                <<"\n\n";

            runner.list_benchmarks();

            return 1;
        }


        return 0;
    }


    runner.run_all();


    return 0;
}