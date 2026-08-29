#include <algorithm>
#include <vector>
#include <iostream>

int main(){

    std::vector<int>a={5,3,1,4,2};

    std::sort(a.begin(),a.end());

    for(int x:a){
        std::cout<<x<<" ";
    }

    return 0;
}