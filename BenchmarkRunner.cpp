#include "BenchmarkRunner.h"
#include "Statistics.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace{

const char*input_case_name(InputDataCase input_case){
    switch(input_case){
    case InputDataCase::Random: return "Random";
    case InputDataCase::Sorted: return "Sorted";
    case InputDataCase::ReverseSorted: return "Reverse";
    }
    return "Unknown";
}

const std::vector<InputDataCase>&input_cases(){
    static const std::vector<InputDataCase>cases={
        InputDataCase::Random,
        InputDataCase::Sorted,
        InputDataCase::ReverseSorted
    };
    return cases;
}

std::string verification_status(const BenchmarkSummary&summary){
    if(!summary.verification_performed){
        return "NOT CHECKED";
    }
    return summary.verified ? "PASS" : "FAIL";
}

std::string format_time(double time_ns){
    std::ostringstream output;
    output<<std::fixed<<std::setprecision(2);
    if(time_ns>=1000000.0){
        output<<time_ns/1000000.0<<" ms";
    }else if(time_ns>=1000.0){
        output<<time_ns/1000.0<<" us";
    }else{
        output<<time_ns<<" ns";
    }
    return output.str();
}

std::string csv_escape(const std::string&value){
    std::string escaped="\"";
    for(char character:value){
        if(character=='\"'){
            escaped+="\"\"";
        }else{
            escaped+=character;
        }
    }
    escaped+='\"';
    return escaped;
}

}

BenchmarkRunner::BenchmarkRunner(int warmup,int iterations){
    warmup_runs=warmup;
    this->iterations=iterations;
    overhead=measure_overhead();
    input_sizes={100,500,1000,2000};
}

void BenchmarkRunner::add(
    const std::string&key,
    const std::string&name,
    BenchmarkSetupFunction setup,
    BenchmarkFunction function,
    BenchmarkVerificationFunction verify
){
    benchmarks.push_back({key,name,setup,function,verify});
}

void BenchmarkRunner::add(
    const std::string&key,
    const std::string&name,
    BenchmarkFunction setup,
    BenchmarkFunction function
){
    add(
        key,
        name,
        [setup](size_t input_size,InputDataCase){ setup(input_size); },
        function
    );
}

void BenchmarkRunner::set_input_sizes(const std::vector<size_t>&sizes){
    input_sizes=sizes;
}

static BenchmarkSummary run_single_benchmark(
    const Benchmark&benchmark,
    size_t input_size,
    InputDataCase input_case,
    int warmup_runs,
    int iterations,
    uint64_t overhead
){
    std::vector<uint64_t>tick_results;
    std::vector<uint64_t>time_results;

    for(int i=0;i<warmup_runs;i++){
        run_benchmark(
            benchmark.setup,
            benchmark.function,
            input_size,
            input_case,
            overhead
        );
    }

    const bool verification_performed=static_cast<bool>(benchmark.verify);
    bool verified=true;

    for(int i=0;i<iterations;i++){
        const BenchmarkResult result=run_benchmark(
            benchmark.setup,
            benchmark.function,
            input_size,
            input_case,
            overhead
        );
        tick_results.push_back(result.cycles);
        time_results.push_back(result.time_ns);

        if(verification_performed && !benchmark.verify()){
            verified=false;
        }
    }

    const double average_ticks=get_average(tick_results);
    const double median_ticks=get_median(tick_results);
    const double average_time=get_average(time_results);
    const double median_time=get_median(time_results);
    const double stddev_ticks=get_standard_deviation(
        tick_results,
        average_ticks
    );
    const double stddev_time=get_standard_deviation(
        time_results,
        average_time
    );
    const uint64_t minimum_ticks=get_minimum(tick_results);
    const uint64_t maximum_ticks=get_maximum(tick_results);
    const uint64_t minimum_time=get_minimum(time_results);
    const uint64_t maximum_time=get_maximum(time_results);

    const BenchmarkSummary summary={
        benchmark.key,
        benchmark.name,
        input_size,
        input_case,
        verification_performed,
        verified,
        average_ticks,
        median_ticks,
        average_time,
        median_time
    };

    std::cout
        <<"----------------------------------------\n"
        <<"Benchmark : "<<benchmark.name<<'\n'
        <<"Input size: "<<input_size<<'\n'
        <<"Input data: "<<input_case_name(input_case)<<'\n'
        <<"Command   : --"<<benchmark.key<<'\n'
        <<"Verification: "<<verification_status(summary)<<'\n'
        <<"----------------------------------------\n"
        <<"Warm-up runs  : "<<warmup_runs<<'\n'
        <<"Measured runs : "<<iterations<<"\n\n"
        <<"RDTSC\n"
        <<"Minimum ticks : "<<minimum_ticks<<'\n'
        <<"Maximum ticks : "<<maximum_ticks<<'\n'
        <<"Average ticks : "<<std::fixed<<std::setprecision(2)
        <<average_ticks<<'\n'
        <<"Median ticks  : "<<median_ticks<<'\n'
        <<"Std deviation : "<<stddev_ticks<<" ticks\n\n"
        <<"High Resolution Timer\n"
        <<"Minimum time  : "<<minimum_time<<" ns\n"
        <<"Maximum time  : "<<maximum_time<<" ns\n"
        <<"Average time  : "<<average_time<<" ns\n"
        <<"Median time   : "<<median_time<<" ns\n"
        <<"Std deviation : "<<stddev_time<<" ns\n\n";

    return summary;
}

static void print_size_comparison(
    const std::vector<BenchmarkSummary>&results,
    size_t input_size,
    InputDataCase input_case
){
    if(results.empty()){
        return;
    }

    std::vector<size_t>ranking;
    for(size_t i=0;i<results.size();i++){
        ranking.push_back(i);
    }

    std::sort(
        ranking.begin(),
        ranking.end(),
        [&results](size_t left,size_t right){
            return results[left].average_time_ns
                <results[right].average_time_ns;
        }
    );

    std::cout
        <<"========================================\n"
        <<" INPUT SIZE: "<<input_size
        <<" | INPUT DATA: "<<input_case_name(input_case)<<"\n"
        <<"========================================\n\n"
        <<std::left
        <<std::setw(8)<<"Rank"
        <<std::setw(25)<<"Benchmark"
        <<std::setw(14)<<"Verified"
        <<std::setw(20)<<"Average Time"
        <<std::setw(18)<<"Average TSC"<<'\n'
        <<"--------------------------------------------------------------------------------\n";

    for(size_t rank=0;rank<ranking.size();rank++){
        const BenchmarkSummary&result=results[ranking[rank]];
        std::ostringstream ticks;
        ticks<<std::fixed<<std::setprecision(2)<<result.average_ticks;

        std::cout
            <<std::left
            <<std::setw(8)<<rank+1
            <<std::setw(25)<<result.name
            <<std::setw(14)<<verification_status(result)
            <<std::setw(20)<<format_time(result.average_time_ns)
            <<std::setw(18)<<ticks.str()<<'\n';
    }

    std::cout
        <<"--------------------------------------------------------------------------------\n\n";
}

struct MeasuredComplexity{
    bool available;
    double exponent;
    size_t sample_count;
};

static MeasuredComplexity estimate_measured_complexity(
    const std::vector<BenchmarkSummary>&summaries,
    const std::string&key,
    InputDataCase input_case
){
    std::vector<double>log_sizes;
    std::vector<double>log_times;

    for(const BenchmarkSummary&summary:summaries){
        if(
            summary.key==key
            && summary.input_case==input_case
            && summary.input_size>1
            && summary.median_ticks>0.0
        ){
            log_sizes.push_back(
                std::log(static_cast<double>(summary.input_size))
            );
            log_times.push_back(std::log(summary.median_ticks));
        }
    }

    if(log_sizes.size()<3){
        return{false,0.0,log_sizes.size()};
    }

    double average_log_size=0.0;
    double average_log_time=0.0;
    for(size_t i=0;i<log_sizes.size();i++){
        average_log_size+=log_sizes[i];
        average_log_time+=log_times[i];
    }
    average_log_size/=log_sizes.size();
    average_log_time/=log_times.size();

    double numerator=0.0;
    double denominator=0.0;
    for(size_t i=0;i<log_sizes.size();i++){
        const double size_difference=log_sizes[i]-average_log_size;
        numerator+=size_difference*(log_times[i]-average_log_time);
        denominator+=size_difference*size_difference;
    }

    if(denominator==0.0){
        return{false,0.0,log_sizes.size()};
    }

    return{
        true,
        std::max(0.0,numerator/denominator),
        log_sizes.size()
    };
}

static std::string format_measured_complexity(
    const MeasuredComplexity&complexity
){
    if(!complexity.available){
        return "Need 3 input sizes";
    }
    if(complexity.exponent<0.15){
        return "O(1)";
    }

    std::ostringstream output;
    output
        <<"O(n^"
        <<std::fixed
        <<std::setprecision(2)
        <<complexity.exponent
        <<")";
    return output.str();
}

static std::string case_verification_status(
    const std::vector<BenchmarkSummary>&summaries,
    const std::string&key,
    InputDataCase input_case
){
    bool checked=false;
    for(const BenchmarkSummary&summary:summaries){
        if(
            summary.key==key
            && summary.input_case==input_case
            && summary.verification_performed
        ){
            checked=true;
            if(!summary.verified){
                return "FAIL";
            }
        }
    }
    return checked ? "PASS" : "NOT CHECKED";
}

static double find_average_time(
    const std::vector<BenchmarkSummary>&summaries,
    const std::string&key,
    InputDataCase input_case,
    size_t input_size
){
    for(const BenchmarkSummary&summary:summaries){
        if(
            summary.key==key
            && summary.input_case==input_case
            && summary.input_size==input_size
        ){
            return summary.average_time_ns;
        }
    }
    return 0.0;
}

static void print_scaling_comparison(
    const std::vector<BenchmarkSummary>&summaries,
    const std::vector<size_t>&input_sizes,
    const std::vector<Benchmark>&benchmarks
){
    if(summaries.empty()){
        return;
    }

    std::cout
        <<"========================================\n"
        <<"      MEASURED PERFORMANCE vs INPUT SIZE\n"
        <<"========================================\n\n"
        <<std::left
        <<std::setw(25)<<"Benchmark"
        <<std::setw(12)<<"Input data"
        <<std::setw(14)<<"Verified"
        <<std::setw(20)<<"Measured growth";

    for(size_t input_size:input_sizes){
        std::ostringstream header;
        header<<"N="<<input_size;
        std::cout<<std::setw(16)<<header.str();
    }

    std::cout
        <<"\n---------------------------------------------------------------------------------------------------------------\n";

    for(const Benchmark&benchmark:benchmarks){
        for(InputDataCase input_case:input_cases()){
            const MeasuredComplexity complexity=
                estimate_measured_complexity(
                    summaries,
                    benchmark.key,
                    input_case
                );

            std::cout
                <<std::left
                <<std::setw(25)<<benchmark.name
                <<std::setw(12)<<input_case_name(input_case)
                <<std::setw(14)<<case_verification_status(
                    summaries,
                    benchmark.key,
                    input_case
                )
                <<std::setw(20)<<format_measured_complexity(complexity);

            for(size_t input_size:input_sizes){
                std::cout
                    <<std::setw(16)
                    <<format_time(find_average_time(
                        summaries,
                        benchmark.key,
                        input_case,
                        input_size
                    ));
            }
            std::cout<<'\n';
        }
    }

    std::cout
        <<"===============================================================================================================\n"
        <<"Measured growth uses median TSC cycles and fits time = C * n^p.\n";
}

static bool write_csv_report(const std::vector<BenchmarkSummary>&summaries){
    std::ofstream report("benchmark_report.csv");
    if(!report){
        return false;
    }

    report
        <<"benchmark_key,benchmark_name,input_data,input_size,verification,"
        <<"average_ticks,median_ticks,average_time_ns,median_time_ns,"
        <<"measured_growth\n";
    report<<std::fixed<<std::setprecision(2);

    for(const BenchmarkSummary&summary:summaries){
        const MeasuredComplexity complexity=
            estimate_measured_complexity(
                summaries,
                summary.key,
                summary.input_case
            );

        report
            <<csv_escape(summary.key)<<','
            <<csv_escape(summary.name)<<','
            <<csv_escape(input_case_name(summary.input_case))<<','
            <<summary.input_size<<','
            <<csv_escape(verification_status(summary))<<','
            <<summary.average_ticks<<','
            <<summary.median_ticks<<','
            <<summary.average_time_ns<<','
            <<summary.median_time_ns<<','
            <<csv_escape(format_measured_complexity(complexity))
            <<'\n';
    }

    return static_cast<bool>(report);
}

void BenchmarkRunner::run_all(){
    summaries.clear();
    const double tsc_frequency=get_tsc_frequency();

    std::cout
        <<"========================================\n"
        <<"       CODE PERFORMANCE ANALYZER V2\n"
        <<"========================================\n\n"
        <<"Timer overhead : "<<overhead<<" TSC ticks\n"
        <<"TSC frequency  : "<<std::fixed<<std::setprecision(3)
        <<tsc_frequency<<" GHz\n\n";

    for(InputDataCase input_case:input_cases()){
        for(size_t input_size:input_sizes){
            std::vector<BenchmarkSummary>size_results;

            for(const Benchmark&benchmark:benchmarks){
                std::cout
                    <<"Starting : "<<benchmark.name
                    <<" | "<<input_case_name(input_case)
                    <<" | N="<<input_size<<'\n';

                const BenchmarkSummary summary=run_single_benchmark(
                    benchmark,
                    input_size,
                    input_case,
                    warmup_runs,
                    iterations,
                    overhead
                );

                summaries.push_back(summary);
                size_results.push_back(summary);

                std::cout
                    <<"Finished : "<<benchmark.name
                    <<" | "<<input_case_name(input_case)
                    <<" | N="<<input_size<<"\n\n";
            }

            print_size_comparison(
                size_results,
                input_size,
                input_case
            );
        }
    }

    print_scaling_comparison(summaries,input_sizes,benchmarks);

    if(write_csv_report(summaries)){
        std::cout<<"CSV report written: benchmark_report.csv\n";
    }else{
        std::cerr<<"Error: could not write benchmark_report.csv\n";
    }
}

bool BenchmarkRunner::run_selected(const std::string&key){
    const double tsc_frequency=get_tsc_frequency();

    for(const Benchmark&benchmark:benchmarks){
        if(benchmark.key==key){
            std::cout
                <<"========================================\n"
                <<"       CODE PERFORMANCE ANALYZER V2\n"
                <<"========================================\n\n"
                <<"TSC frequency : "<<std::fixed<<std::setprecision(3)
                <<tsc_frequency<<" GHz\n\n";

            std::vector<BenchmarkSummary>selected_summaries;

            for(InputDataCase input_case:input_cases()){
                for(size_t input_size:input_sizes){
                    std::cout
                        <<"Running "<<benchmark.name
                        <<" | "<<input_case_name(input_case)
                        <<" | N="<<input_size<<"\n";

                    selected_summaries.push_back(run_single_benchmark(
                        benchmark,
                        input_size,
                        input_case,
                        warmup_runs,
                        iterations,
                        overhead
                    ));

                    std::cout<<'\n';
                }
            }

            for(InputDataCase input_case:input_cases()){
                const MeasuredComplexity complexity=
                    estimate_measured_complexity(
                        selected_summaries,
                        benchmark.key,
                        input_case
                    );

                std::cout
                    <<"Measured growth ("<<input_case_name(input_case)
                    <<"): "<<format_measured_complexity(complexity)
                    <<'\n';
            }

            if(write_csv_report(selected_summaries)){
                std::cout<<"CSV report written: benchmark_report.csv\n";
            }else{
                std::cerr<<"Error: could not write benchmark_report.csv\n";
            }

            return true;
        }
    }

    return false;
}

void BenchmarkRunner::list_benchmarks() const{
    std::cout<<"Available benchmarks:\n\n";

    for(const Benchmark&benchmark:benchmarks){
        std::cout
            <<"  --"
            <<std::left
            <<std::setw(12)<<benchmark.key
            <<benchmark.name<<'\n';
    }

    std::cout<<"\nInput sizes:\n";
    for(size_t size:input_sizes){
        std::cout<<"  N="<<size<<'\n';
    }

    std::cout
        <<"\nInput data: Random, Sorted, Reverse\n"
        <<"Each sorting result is verified after timing.\n"
        <<"CSV report: benchmark_report.csv\n"
        <<"\n  --all         Run all benchmarks\n"
        <<"  --help        Show available benchmarks\n"
        <<"\nMeasured growth needs at least 3 input sizes.\n";
}
