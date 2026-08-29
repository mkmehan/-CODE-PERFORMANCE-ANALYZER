#include "Statistics.h"

#include <algorithm>
#include <cmath>


double get_average(
    const std::vector<uint64_t>&results
){

    if(results.empty()){

        return 0.0;
    }


    long double sum=0.0;


    for(uint64_t value:results){

        sum+=value;
    }


    return (double)(
        sum/results.size()
    );
}


uint64_t get_minimum(
    const std::vector<uint64_t>&results
){

    if(results.empty()){

        return 0;
    }


    return *std::min_element(
        results.begin(),
        results.end()
    );
}


uint64_t get_maximum(
    const std::vector<uint64_t>&results
){

    if(results.empty()){

        return 0;
    }


    return *std::max_element(
        results.begin(),
        results.end()
    );
}


double get_median(
    std::vector<uint64_t>results
){

    if(results.empty()){

        return 0.0;
    }


    std::sort(
        results.begin(),
        results.end()
    );


    size_t n=results.size();


    if(n%2==1){

        return results[n/2];
    }


    return (
        results[n/2-1]+
        results[n/2]
    )/2.0;
}


double get_standard_deviation(
    const std::vector<uint64_t>&results,
    double average
){

    if(results.empty()){

        return 0.0;
    }


    long double sum=0.0;


    for(uint64_t value:results){

        long double difference=
            (long double)value-average;


        sum+=
            difference*difference;
    }


    return sqrt(
        (double)(
            sum/results.size()
        )
    );
}