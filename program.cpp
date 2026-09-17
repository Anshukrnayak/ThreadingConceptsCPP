#include<iostream>
#include<queue>
#include<vector> 
#include<string> 


struct ProcessState{
    int processId;
    std::string name;
    ProcessState(int processId,std::string name) : processId(processId), name(name) {}; 
}; 



struct ProcessControlBlock{
    int processId;
    ProcessState processState; 
    int programCounter;
    std::string instruction; 
};


int main(){
    
    std::vector<ProcessState> pStates; 
    std::vector<std::string> stateList={"New","Ready","Runing","Waiting","Terminated"}; 
    
    for(int i=0;i<stateList.size();i++){
        ProcessState newState(i,stateList[i]);
        pStates.push_back(newState);
    }


    for(const auto& data : pStates){
        std::cout<<" Process ID "<<data.processId; 
        std::cout<<" Process Name :  "<<data.name<<std::endl;
    }


    return 0;
}
