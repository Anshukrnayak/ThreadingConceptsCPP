#include<iostream>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<thread>
#include<chrono>
#include<queue>


static int count=0; 
std::condition_variable gConditionalVariable;
std::mutex gLock; 
bool notified=false; 
std::queue<int> queue; 


auto worker=[](){

    std::unique_lock<std::mutex> lock(gLock);
    count++; 
    queue.push(count);
    notified=true; 
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout<<" Work has done : "<<std::endl;
    
    // Notify after completing task or work : 
    gConditionalVariable.notify_one();

};


auto consumer=[](){

    std::unique_lock<std::mutex> lock(gLock);
    if(!notified){
        // wating for thread unlock and done the task. 
        gConditionalVariable.wait(lock); 
    }else{
        while(!queue.empty()){
            std::cout<<" "<<queue.front();
            queue.pop();
        } 
    }

};


int main(){
    
    std::thread work(worker); 
    std::thread consume(consumer);

    work.join();
    consume.join();
    

    return 0;
}
