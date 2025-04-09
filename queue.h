#include <atomic>
#include <optional>

template<class T>
struct Node {
    std::atomic<T*> data;
    std::atomic<Node<T>*> next;

    Node():data{nullptr}, next{nullptr} {}
    ~Node() {delete data.load()};//next is a pointer whose value will deallocate and destruct itself
};

template<class T>
class Queue {//Micheal Scott queue
    std::atomic<Node<T>*> head;
    std::atomic<Node<T>*> tail;
    std::atomic<Node<T>*> toDelete;//This is a lock-free bag of nodes saving nodes for deletion
    std::atomic<int> threadsInPop;
    void restoreQueue();
    void tryReclaim(Node<T>* oldHead);
public:
    Queue(): {
        head.store(new Node<T>());
		tail = head.load();
        toDelete.store(nullptr);
		threadsInPop.store(0);
    }
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

    Node<T>* current = toDelete;
    while (current != nullptr) { //perform the actual deletion
        Node<T>* next = current->next;
        delete current;
        current = next;
    }
}

template<class T>
void Queue<T>::restoreQueue() {
    Node<T>* newNode = new Node();
	Node<T>* oldTail = tail;//all operations must be taken on a local copy of tail
	Node<T>* null = nullptr;

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
    T* null = nullptr;
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
void Queue<T>::tryReclaim(Node<T>* oldHead) {
    if (threadsInPop == 1) {//only thread in pop
		Node<T>* oldToDelete = toDelete.exchange(nullptr);//claim the toDelete bag
		if (!--threadsInPop) {//sole thread with access to the nodes in toDelete
			//delete everything in the bag
            while (oldToDelete != nullptr) {
				Node<T>* current = oldToDelete;
				oldToDelete = oldToDelete->next;
				delete current;
            }
		}
        else {
            //replace the nodes
            while (current != nullptr) {
			Node<T>* current = oldToDelete;
			oldToDelete = current->next;
			Node<T>* curToDelete = toDelete;
			current->next = curToDelete;
            while (!toDelete.compare_exchange_weak(curToDelete, current));
            }
        }
        delete oldHead;//this is safe to delete because it is not in either list
    }
    else {
        //put oldHead in the toDelete bag
        oldHead->next = toDelete;
        while (!toDelete.compare_exchange_weak(oldHead->next, oldHead));
        --threadsInPop;
    }
}

template<class T>
std::optional<T> Queue<T>::pop() {
    if(empty()) return std::nullopt;//Cannot pop the dummy node
	threadsInPop++;

    Node<T>* oldHead = head;
    do {
        if (oldHead->next == nullptr) return std::nullopt;//Don't pop the dummy node
    } while (!head.compare_exchange_weak(oldHead, oldHead->next));

    T data = *(oldHead->data);
    tryReclaim(oldHead);

    return data;
}
