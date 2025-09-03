//
// Created by Supradeep Chitumalla on 03/09/25.
//

#ifndef STACK_H
#define STACK_H
#include <atomic>
#include <memory>

template<typename T>
class LockFreeStack {
private:
    struct Node;
    struct CountedNodePtr {
        int externalCount;
        Node *ptr;
    };

    struct Node {
        std::shared_ptr<T> data;
        std::atomic<int> internalCount;
        CountedNodePtr next;
        explicit Node(T const &data_) : data(std::make_shared<T>(data_)), internalCount(0) {}
    };

    std::atomic<CountedNodePtr> head;

    void increaseHeadCount(CountedNodePtr &oldCounter) {
        CountedNodePtr newCounter;
        do {
            newCounter = oldCounter;
            ++newCounter.externalCount;
        }
        while (!head.compare_exchange_strong(oldCounter, newCounter,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed));
        oldCounter.externalCount = newCounter.externalCount;
    }

public:
    ~LockFreeStack() {
        while (pop());
    }

    void push(T const &data) {
        CountedNodePtr newNode;
        newNode.ptr = new Node(data);
        newNode.externalCount = 1;
        newNode.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_strong(newNode.ptr->next, newNode,
                                           std::memory_order_release,
                                           std::memory_order_relaxed));
    }

    std::shared_ptr<T> pop() {
        CountedNodePtr oldHead = head.load(std::memory_order_relaxed);
        for(;;) {
            increaseHeadCount(oldHead);
            Node* const ptr = oldHead.ptr;
            if(!ptr) {
                return std::shared_ptr<T>();
            }
            if(head.compare_exchange_strong(oldHead, ptr->next,
                                          std::memory_order_relaxed)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int const countIncrease = oldHead.externalCount - 2;
                if(ptr->internalCount.fetch_add(countIncrease,
                                              std::memory_order_release) == -countIncrease) {
                    delete ptr;
                }
                return res;
            }
            else if(ptr->internalCount.fetch_add(-1, std::memory_order_relaxed) == 1) {
                ptr->internalCount.load(std::memory_order_acquire);
                delete ptr;
            }
        }
    }
};

#endif