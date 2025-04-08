#include <atomic>
#include <optional>

template<class T>
struct Node {
    std::atomic<T*> data;
    std::atomic<Node*> next;

    Node():data{nullptr}, next{nullptr} {}
    ~Node() {delete data.load()};//next is a pointer whose value will deallocate and destruct itself
};

template<class T>
class Queue {//Micheal Scott queue
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    std::atomic<Node*> toDelete;//This is a lock-free bag of nodes saving nodes for deletion
    void restoreQueue();
public:
    Queue(): head{new Node}, tail{head.load()}, toDelete{nullptr} {}
    ~Queue();
    void push(T data);
    std::optional<T> pop();
    bool empty() {return head->next == nullptr};
};

template<class T>
Queue<T>::~Queue() {
    while (!empty()) {
        pop();//just dump everything into toDelete
    }
    delete head;//delete the dummy node

    Node* current = toDelete;
    while (current != nullptr) { //perform the actual deletion
        Node* next = current->next;
        delete current;
        current = next;
    }
}

template<class T>
void Queue<T>::restoreQueue() {
    Node* newNode = new Node();
	Node* oldTail = tail;//all operations must be taken on a local copy of tail
	Node* null = nullptr;

    while (oldTail->data != nullptr) {
        if (oldTail->next.compare_exchange_strong(null, newNode)) { //second linearization point
            newNode = new Node();//newNode always points to a new node. There will always be one extra newNode allocation
        }
        else { //null needs to be reset if the CAS fails
            null = nullptr;
        }

		tail.compare_exchange_strong(oldTail, oldTail->next);//third linearization point

        oldTail = tail;//oldTail is reset for the check to make sure we are in a stable state
    }
    delete newNode;
}

template<class T>
void Queue<T>::push(T data) {
    T* newData = new T (std::move(data));
    bool inserted = false;
    Node* null = nullptr;
    while (!inserted) {
        if (tail->data.compare_exchange_strong(null, newData)) { //first linearization point
            inserted = true;
        }
		else { //null needs to be reset if the CAS fails
            null = nullptr;
        }

        restoreQueue();
    }
}

template<class T>
std::optional<T> Queue<T>::pop() {
    if(empty()) return std::nullopt;//Cannot pop the dummy node

    Node* oldHead = head;
    do {
        if (oldHead->next == nullptr) return std::nullopt;//Don't pop the dummy node
    } while (!head.compare_exchange_weak(oldHead, oldHead->next));

    oldHead->next = toDelete;
    while(!toDelete.compare_exchange_weak(oldHead->next, oldHead));

    return *(oldHead->data);
}
