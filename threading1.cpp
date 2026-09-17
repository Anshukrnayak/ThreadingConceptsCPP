#include<iostream>
#include<vector>
#include<thread>
#include<mutex>


static int count=0; 
std::mutex gLock; 

auto worker=[](){
    // atomic value :
    std::unique_lock<std::mutex> unique_lock(gLock);
    count++; 
};


auto consumer=[](){
    std::unique_lock<std::mutex> unique_lock(gLock);
    std::cout<<" value of count : "<<count<<std::endl; 
};


int main(){

    std::vector<std::thread> workerThreads; 
    std::vector<std::thread> consumerThreads; 


    for(int i=0;i<10;i++){
        workerThreads.push_back(std::thread(worker));
    }

    for(int i=0;i<10;i++){
        consumerThreads.push_back(std::thread(consumer));
    }
    
   for(auto& thread : consumerThreads){
       thread.join();
   } 

   for(auto& thread : workerThreads){
       thread.join();
   }

    
    return 0;
}
