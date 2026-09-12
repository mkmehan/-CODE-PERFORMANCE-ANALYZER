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

double get_percentile(
    std::vector<uint64_t> values,
    double percentile
){
    if(values.empty()){
        return 0.0;
    }

    percentile=std::max(0.0,std::min(100.0,percentile));
    std::sort(values.begin(),values.end());

    if(values.size()==1){
        return static_cast<double>(values.front());
    }

    // Linear interpolation gives well-defined percentile values for small
    // benchmark samples as well as the usual 100+ measurement runs.
    const double position=percentile/100.0*(values.size()-1);
    const size_t lower=static_cast<size_t>(std::floor(position));
    const size_t upper=static_cast<size_t>(std::ceil(position));
    const double fraction=position-lower;

    return static_cast<double>(values[lower])+
        (static_cast<double>(values[upper])-static_cast<double>(values[lower]))*
        fraction;
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

double get_coefficient_of_variation(
    const std::vector<uint64_t>& results,
    double average
){
    if(results.empty() || average==0.0){
        return 0.0;
    }

    return get_standard_deviation(results,average)/std::abs(average);
}
