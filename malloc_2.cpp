#include <unistd.h>
#include <cmath>
#include <iostream>
#include <string.h>
using namespace std;

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    void* address;
};

struct MallocMetadata dummy_head = {
    0, false, nullptr, nullptr,nullptr
};

struct MallocMetadata* find_block(size_t size){

    struct MallocMetadata* block = &dummy_head;
    block = block->next;

    if(!block){
        return nullptr;
    }

    while(block){
        if(block->is_free && block->size >= size){
            return block;
        }
        block = block->next;
    }

    return nullptr;

}

//// ---1---
void* smalloc(size_t size){

//    cout << "---1---" << endl;
    if(size == 0){
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }
//    cout << "---2---" << endl;
    struct MallocMetadata *block = find_block(size);
//    cout << "---3---" << endl;
    if(!block){

        size_t size_with_metadata = size + sizeof (MallocMetadata);

        void* first_byte = sbrk(size_with_metadata);
        if(first_byte == (void*)-1){
            return nullptr;
        }

//        cout << "---4---" << endl;
        struct MallocMetadata* temp_block = &dummy_head;
        while(temp_block->next){
            temp_block = temp_block->next;
        }
//        cout << "---5---" << endl;

        struct MallocMetadata node;
        node.size = size;
        node.is_free = false;
        node.next = nullptr;
        node.prev = temp_block;
        node.address = first_byte;

        temp_block->next = (MallocMetadata*)first_byte;
//        cout << "---6---" << endl;
        *((MallocMetadata*)first_byte) = node;
//        cout << "---7---" << endl;
        return (void*)((size_t)node.address + sizeof(MallocMetadata));

    }
    else {
        block->is_free = false;
        return (void*)((size_t)block->address + sizeof(MallocMetadata));
    }

}


//// ---2---
void* scalloc(size_t num, size_t size){
    if(size == 0 || num == 0){
        return nullptr;
    }
    if(num*size > pow(10, 8)){
        return nullptr;
    }

    void* address = smalloc(num*size);
    if (address == nullptr) {
        return nullptr;
    }
    memset(address, 0, num * size);
    return address;
}


//// ---3---
void sfree(void* p) {
    if (p == nullptr) {
        return;
    }

    //find the address of Metadata block, and update "is_free == 1"
    void* address_metadata = (void*)((size_t)p - sizeof(MallocMetadata));

    struct MallocMetadata* block = (MallocMetadata*)address_metadata;
    if (block->is_free) {
        return;
    }
    block->is_free = 1;
}


//// ---4---
void* srealloc(void* oldp, size_t size) {

//    cout << "--1--" << endl;
    if (size == 0) {
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }
    if (!oldp) {
        return smalloc(size);
    }

//    cout << "--2--" << endl;


    //find the address of old Metadata block
    void* old_address_metadata = (void*)((size_t)oldp - sizeof(MallocMetadata));
    struct MallocMetadata* block = (MallocMetadata*)old_address_metadata;

//    cout << "--3--" << endl;
    if (block->size >= size) {
        return oldp;
    }
//    cout << "--4--" << endl;

    block->is_free = 1;

    //allocate a new block with the correct size
    void* new_address_block = smalloc(size);

//    cout << "--5--" << endl;
    //move the original data to the new block
    memmove(new_address_block, oldp, size);

//    cout << "--6--" << endl;

    return new_address_block;
}


//// ---5---
size_t _num_free_blocks(){

    size_t counter = 0;
    struct MallocMetadata *block = &dummy_head;

    // start from first block - not from dummy
    block = block->next;

    while(block){
        if(block->is_free){
            counter++;
        }
        block = block->next;
    }

    return counter;
}

//// ---6---
size_t _num_free_bytes(){

    size_t free_bytes = 0;

    struct MallocMetadata *block = &dummy_head;
    block = block->next;

    // count free bytes without metadata
    while(block){
        if(block->is_free){
            free_bytes += block->size;
        }
        block = block->next;
    }

    return free_bytes;

}


//// ---7---
size_t _num_allocated_blocks(){

//    cout << "--start _num_allocated_blocks--" << endl;
    size_t counter = 0;
    struct MallocMetadata *block = &dummy_head;

    // start from first block - not from dummy
    block = block->next;

//    cout << "--before while loop--" << endl;
    while(block){
        counter++;
        block = block->next;
    }
//    cout << "--after while loop--" << endl;

    return counter;

}

//// ---8---
size_t _num_allocated_bytes(){

    size_t allocated_bytes = 0;

    struct MallocMetadata *block = &dummy_head;
    block = block->next;

    // count free bytes without metadata
//    cout << "--before while loop--" << endl;
    while(block){
//        cout << '+' << endl;
        allocated_bytes += block->size;
        block = block->next;
    }

    return allocated_bytes;
}

//// ---10---
size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

//// ---9---
size_t _num_meta_data_bytes(){

    int num_allocated_blocks = _num_allocated_blocks();
    int size_metadata = _size_meta_data();
    return num_allocated_blocks * size_metadata;

}


int main(){


    size_t metadata_size = _size_meta_data();
    cout << "metadata_size : " << metadata_size << endl;
//    void *base = sbrk(0);
//    char *a = (char *)smalloc(10);
//
//    char *b = (char *)smalloc(10);
//
//    char *c = (char *)smalloc(10);
//
//
//    sfree(b);
//    sfree(a);
//    sfree(c);
//
//    char *new_a = (char *)smalloc(10);
//
//    char *new_b = (char *)smalloc(10);
//
//    char *new_c = (char *)smalloc(10);
//
//    //    cout << "---finished allocation---" << endl;
//    int num_allocated = _num_allocated_blocks();
////    cout << "---num allocated blocks---" << endl;
//    int num_bytes = _num_allocated_bytes();
////    cout << "---num allocated bytes---" << endl;
//    int num_free = _num_free_blocks();
//    int num_free_bytes = _num_free_bytes();
//
//    cout << "num allocated blocks: " << num_allocated << endl;
//    cout << "num allocated bytes: " << num_bytes << endl;
//    cout << "num free blocks: " << num_free << endl;
//    cout << "num free bytes: " << num_free_bytes << endl;


    return 0;
}

