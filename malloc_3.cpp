#include <unistd.h>
#include <cmath>
#include <iostream>
#include <string.h>
using namespace std;

#define MAX_ORDER 10

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata* next_free;
    MallocMetadata* prev_free;
    void* address;
};

MallocMetadata* free_array[MAX_ORDER+1];

struct MallocMetadata dummy_head = {
        0, false, nullptr, nullptr,nullptr
};

bool is_first_malloc = true;

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

    if(is_first_malloc){

        // align allocation
        void* curr_program_break = sbrk(0);
        size_t reminder = (size_t)curr_program_break % (32*128*1024);
        size_t difference = 32*128*1024 - reminder;
        void* address_of_initial_allocation = sbrk(32*128*1024 + difference);


        void* address_of_aligned_allocation = (void*)((size_t)address_of_initial_allocation + difference);

        // save metadata to blocks
        for(int i = 0; i<32; i++){
            struct MallocMetadata node = {
                    128*1024, true, nullptr, nullptr,nullptr,
                    nullptr, (void*)((size_t)address_of_initial_allocation + i*128*1024)
            };
            *(MallocMetadata*)((size_t)address_of_initial_allocation + i*128*1024) = node;
        }

        // set lists pointers
        for(int i = 0; i<32; i++){
            MallocMetadata* curr_metadata_address =  (MallocMetadata*)((size_t)address_of_initial_allocation + i*128*1024);
           if(i == 0){
               free_array[MAX_ORDER]->next_free = curr_metadata_address;
               curr_metadata_address->prev_free = free_array[MAX_ORDER];
               curr_metadata_address->next = (MallocMetadata*)((size_t)address_of_initial_allocation + (i+1)*128*1024);
               curr_metadata_address->prev = &dummy_head;
           } else if(i == 31) {
               curr_metadata_address->prev = (MallocMetadata*)((size_t)address_of_initial_allocation + (i-1)*128*1024);
               curr_metadata_address->prev_free = (MallocMetadata*)((size_t)address_of_initial_allocation + (i-1)*128*1024);
           } else {
               curr_metadata_address->next = (MallocMetadata*)((size_t)address_of_initial_allocation + (i+1)*128*1024);
               curr_metadata_address->next_free = (MallocMetadata*)((size_t)address_of_initial_allocation + (i+1)*128*1024);
               curr_metadata_address->prev = (MallocMetadata*)((size_t)address_of_initial_allocation + (i-1)*128*1024);
               curr_metadata_address->prev_free = (MallocMetadata*)((size_t)address_of_initial_allocation + (i-1)*128*1024);
           }

        }
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

    cout << "size = " << _size_meta_data() << endl;

    return 0;
}

