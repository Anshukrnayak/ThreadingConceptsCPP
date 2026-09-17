#include<iostream>
#include<vector>
#include<thread>
#include<mutex>


std::mutex gLock; 

struct Node{
    int data;
    Node* next;
    Node* previous; 

    Node(int data) : data(data), next(nullptr), previous(nullptr) {};
};


struct Queue{
    int count;
    Node* rear;
    Node* front; 

    Queue() : rear(nullptr), front(nullptr), count(0) {};


    void push_front(int data){
        Node* newNode=new Node(data);
        count++; 

        if(front==nullptr){
            front=rear=newNode; 
        }else{
            newNode->next=front;
            front->previous=newNode; 
            front=newNode; 
        }
    }

    void push_back(int data){
        Node* newNode=new Node(data);
        count++; 

        if(rear==nullptr){
            rear=front=newNode; 
        }else{
            newNode->previous=rear;
            rear->next=newNode; 
            rear=newNode; 
        }
    }


    void pop_back(){
        if(rear==nullptr){
            return; 
        }else{
            Node* temp=rear; 
            rear=rear->previous; 
            rear->next=nullptr; 
            if(temp->previous) temp->previous=nullptr;

            delete temp; 
            return; 

        }
    }


    void pop_front(){
        if(front==nullptr){
            return; 
        }else{
            Node* temp=front; 
            front=front->next; 
            front->previous=nullptr; 
            if(temp->next){
                temp->next=nullptr;
            }

            delete temp; 
            return; 
        }
    }

};



void producerThread(Queue* q, int data){
        std::lock_guard<std::mutex>  lockGuard(gLock);   // critical section : 
        q->push_front(data);
        q->push_back(data);
}


void consumerThread(Node* front,Node* rear){
    if(front==nullptr || rear==nullptr){
        return; 
    }
    
    std::lock_guard<std::mutex> lockGuard(gLock); // critical section : 
    while(front!=nullptr && rear!=nullptr){
        std::cout<<" front :  "<<front->data<<" rear  :  "<<rear->data; 

        front=front->next;
        rear=rear->previous; 
    }
}


void printQueue(Node* head){
    if(head==nullptr){
        return; 
    }
    while(head!=nullptr){
        std::cout<<" "<<head->data;
        head=head->next; 
    }
}


int main(){
    Queue* q=new Queue; 
    std::vector<std::thread> threads; 
    
    for(int i=0;i<10;i++){
        threads.push_back(std::thread(producerThread,q,i)); 
    }

    for(auto& thread : threads){
        thread.join();
    }

    printQueue(q->front);
    std::thread consumerT(consumerThread,q->front,q->rear);
    
    consumerT.join();

    return 0;
}
