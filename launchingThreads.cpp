#include<iostream> 
#include<vector> 
#include<thread> 
#include<mutex> 

/// Adding mutual exlusion that mean one thread will access shared memory at a time. 


std::mutex gLock; 

static int count=0; 

auto increase_count=[](){

    // adding lock on shared value : 
    gLock.lock();
    count++;  // critical section :  
    gLock.unlock(); 

    // free the shared value for other threads : 
};




int main(){
    
    std::vector<std::thread> threads; 

    for(int i=0;i<10000;i++){
        threads.push_back(std::thread(increase_count));
    }
    
    for(auto& thread : threads){
        thread.join();
    }

    std::cout<<" value of count : "<<count;

    return 0;
}
