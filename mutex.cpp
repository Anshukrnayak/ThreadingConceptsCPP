#include<iostream>
#include<vector> 
#include<thread>
#include<mutex> 


std::mutex gLock; 
static int count=0; 

//auto lambda=[](){
//    gLock.lock();
//    count++; 
//    gLock.unlock();
// };



auto lambda=[](){
    std::lock_guard<std::mutex> lockGuard(gLock); 
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
