#include<iostream>
#include<vector> 
#include<thread>
#include<mutex> 
#include<atomic> 



std::mutex gLock; 
static std::atomic<int> count=0; 

//auto lambda=[](){
//    gLock.lock();
//    count++; 
//    gLock.unlock();
// };



auto lambda=[](){ 
    count++; // critical section : 
};



int main(){

    std::vector<std::thread> threads; 
    
    for(int i=0;i<100000;i++){
        threads.push_back(std::thread(lambda));
    }


    for(auto& thread : threads){
        thread.join();
    }

    std::cout<<" value of counter : or shared resource :  "<<count; 


    return 0;
}
