#include<iostream>
#include<thread>
#include<vector>
#include<future>



auto square=[](int& x){
    return x*x;
}; 


int main(){
    
    int n=10;
    std::future<int> asyncFunction=std::async(std::ref(square),std::ref(n));
    
    for(int i=0;i<100;i++){
        std::future<int> asyncFn=std::async(std::ref(square),std::ref(i)); 
        std::cout<<" value of "<<asyncFn.get()<<std::endl;
    }

    int result=asyncFunction.get();
    std::cout<<" value of square of n :: "<<result;

    return 0;
}
