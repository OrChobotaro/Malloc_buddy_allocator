#include <unistd.h>
#include <cmath>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <cstdlib>
#include <time.h>
using namespace std;

#define MAX_ORDER 10

int mmap_number_bytes = 0;
int mmap_number_blocks = 0;

int rand_cookie;

struct MallocMetadata {
    int cookie;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata* next_free;
    MallocMetadata* prev_free;
    void* address;
};

MallocMetadata free_array[MAX_ORDER+1];
MallocMetadata arr[32];
bool is_first_malloc = true;

struct MallocMetadata dummy_head = {
        -1, 0, false, nullptr, nullptr,nullptr
};



//// functions declarations:
struct MallocMetadata* find_block(size_t size);
int find_order(size_t size_with_metadata);
MallocMetadata* get_best_fit(int order, int* best_fit_order);
size_t _num_free_blocks();
size_t _num_allocated_blocks();
void validate_cookie(MallocMetadata* metadata);




//// ---1---
void* smalloc(size_t size){


    if(is_first_malloc){

        time_t t;
        srand((unsigned)time(&t));
        rand_cookie = rand();


        is_first_malloc = false;
        // align allocation
        void* curr_program_break = sbrk(0);
        size_t reminder = (size_t)curr_program_break % (32*128*1024);
        size_t difference = 32*128*1024 - reminder;
        void* address_of_initial_allocation = sbrk(32*128*1024 + difference);


        void* address_of_aligned_allocation = (void*)((size_t)address_of_initial_allocation + difference);

        // save metadata to memory blocks
        for(int i = 0; i<32; i++){

            arr[i].cookie = rand_cookie;
            arr[i].size = 128*1024;
            arr[i].is_free = true;
            arr[i].next_free = nullptr;
            arr[i].next = nullptr;
            arr[i].prev_free = nullptr;
            arr[i].prev = nullptr;
            arr[i].address = (void*)((size_t)address_of_aligned_allocation + i*128*1024);
            *(MallocMetadata*)((size_t)address_of_aligned_allocation + i*128*1024) = arr[i];
        }

        // initialize free_array:
        for(int i = 0; i < MAX_ORDER+1; i++){
            struct MallocMetadata dummy_node = {
                    rand_cookie, 0, false, nullptr, nullptr,nullptr,
                    nullptr, nullptr
            };

            free_array[i] = dummy_node;
            free_array[i].cookie = rand_cookie;
            free_array[i].size = 0;
            free_array[i].is_free = false;
            free_array[i].next_free = nullptr;
            free_array[i].next = nullptr;
            free_array[i].prev_free = nullptr;
            free_array[i].prev = nullptr;
            free_array[i].address = nullptr;

        }

        // set lists pointers
        for(int i = 0; i<32; i++){
            MallocMetadata* curr_metadata_address =  (MallocMetadata*)((size_t)address_of_aligned_allocation + i*128*1024);
            validate_cookie(curr_metadata_address);
            if(i == 0){
                free_array[MAX_ORDER].next_free = curr_metadata_address;
                curr_metadata_address->prev_free = &free_array[MAX_ORDER];
                curr_metadata_address->next_free = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i+1)*128*1024);
                curr_metadata_address->next = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i+1)*128*1024);
                dummy_head.next = curr_metadata_address;
                curr_metadata_address->prev = &dummy_head;
            } else if(i == 31) {
                curr_metadata_address->prev = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i-1)*128*1024);
                curr_metadata_address->prev_free = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i-1)*128*1024);
            } else {
                curr_metadata_address->next = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i+1)*128*1024);
                curr_metadata_address->next_free = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i+1)*128*1024);
                curr_metadata_address->prev = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i-1)*128*1024);
                curr_metadata_address->prev_free = (MallocMetadata*)((size_t)address_of_aligned_allocation + (i-1)*128*1024);
            }

        }
    }




    if(size == 0){
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }


    size_t size_with_metadata = size + sizeof (MallocMetadata);

    // find the order of wanted block
    int order = find_order(size_with_metadata);

    if(order < 0){ // if size > 128KB
        void* address_mmap = mmap(NULL, size_with_metadata, PROT_EXEC | PROT_WRITE | PROT_READ ,MAP_ANON | MAP_PRIVATE, -1, 0);
        if(address_mmap < 0){
            return nullptr;
        }
        mmap_number_blocks++;
        mmap_number_bytes += size_with_metadata;

        struct MallocMetadata mmap_block_metadata = {
                rand_cookie, size_with_metadata, false, nullptr, nullptr, nullptr,
                nullptr, (void*)address_mmap
        };

        *(MallocMetadata*)address_mmap = mmap_block_metadata;

        return (void*)((size_t)address_mmap + sizeof(MallocMetadata));
    }



    int best_fit_order = 0;
    MallocMetadata* best_fit_metadata = get_best_fit(order, &best_fit_order);
    best_fit_metadata = free_array[best_fit_order].next_free;

    if(!best_fit_metadata){
        return nullptr;
    }


    while(best_fit_order > order){

        validate_cookie(best_fit_metadata);
        struct MallocMetadata splitted_block_metadata = {
                rand_cookie, best_fit_metadata->size/2, true, nullptr, nullptr,nullptr,
                nullptr, (void*)((size_t)best_fit_metadata->address + best_fit_metadata->size/2)
        };
        // update first half metadata
        best_fit_metadata->size /= 2;



        // remove from linked list

        free_array[best_fit_order].next_free = best_fit_metadata->next_free;
        if(best_fit_metadata->next_free){
            best_fit_metadata->next_free->prev_free = &free_array[best_fit_order];
        }


        // add to new linked list
        MallocMetadata* tmp = free_array[best_fit_order-1].next_free;
        MallocMetadata* prev_tmp = &free_array[best_fit_order-1];
        while(tmp){
            validate_cookie(tmp);
            if(tmp->address < best_fit_metadata->address){
                prev_tmp = tmp;
                tmp = tmp->next_free;
                continue;
            }
            break;
        }

        // if first half should be last node
        if(!tmp){
            tmp = prev_tmp;
        } else {
            validate_cookie(tmp->next_free);
            tmp->next_free->prev_free = (MallocMetadata*)(splitted_block_metadata.address);
        }


        splitted_block_metadata.next_free = tmp->next_free;
        tmp->next_free = best_fit_metadata;
        best_fit_metadata->prev_free = tmp;
        best_fit_metadata->next_free = (MallocMetadata*)(splitted_block_metadata.address);
        splitted_block_metadata.prev_free = best_fit_metadata;



        // add to original list

        splitted_block_metadata.next = best_fit_metadata->next;
        if(best_fit_metadata->next){
            validate_cookie(best_fit_metadata->next);
            best_fit_metadata->next->prev = (MallocMetadata*)splitted_block_metadata.address;
        }
        best_fit_metadata->next = (MallocMetadata*)splitted_block_metadata.address;
        splitted_block_metadata.prev = best_fit_metadata;




        // save new metadata to memory
        *(MallocMetadata*)(splitted_block_metadata.address) = splitted_block_metadata;


        best_fit_metadata = get_best_fit(order, &best_fit_order);

    }

    best_fit_metadata->is_free = false;

    // remove best fit
    free_array[best_fit_order].next_free = best_fit_metadata->next_free;
    if(best_fit_metadata->next_free){
        validate_cookie(best_fit_metadata->next_free);
        best_fit_metadata->next_free->prev_free = &free_array[best_fit_order];
    }

    return (void*)((size_t)best_fit_metadata->address + (size_t)sizeof(MallocMetadata));
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



int calc_entry_for_free_array(int size) {
    int cur_size = 64;
    for (int i=0; i<MAX_ORDER+1 ; i++) {
        cur_size = 2*cur_size;
        if (size == cur_size) {
            return i;
        }
    }
    return -1;
}


//// ---3---
void sfree(void* p) {


    if (p == nullptr) {
        return;
    }

    //find the address of Metadata block
    void* address_metadata = (void*)((size_t)p - sizeof(MallocMetadata));
    struct MallocMetadata* metadata_info = (MallocMetadata*)address_metadata;

    //if a block is already free - return
    validate_cookie(metadata_info);
    if (metadata_info->is_free) {
        return;
    }

    if (metadata_info->size > 1024*128) {


        munmap(p, metadata_info->size);
        mmap_number_bytes -= (metadata_info->size);
        mmap_number_blocks-= 1;
        return;
    }

    void* address_metadata_buddy = (void*)((size_t)address_metadata ^ metadata_info->size);
    struct MallocMetadata* metadata_info_buddy = (MallocMetadata*)address_metadata_buddy;
    validate_cookie(metadata_info_buddy);
    int order = calc_entry_for_free_array(metadata_info_buddy->size);


    if (!metadata_info_buddy->is_free || order == MAX_ORDER) {
        metadata_info->is_free = true;
        MallocMetadata* current_metadata = free_array[order].next_free;
        MallocMetadata* prev = &free_array[order];
        while(current_metadata && current_metadata->address < metadata_info->address){
            validate_cookie(current_metadata);
            prev = current_metadata;
            current_metadata = current_metadata->next_free;
        }
        if(current_metadata){
            current_metadata->prev_free = metadata_info;
        }
        metadata_info->next_free = current_metadata;
        prev->next_free = metadata_info;
        metadata_info->prev_free = prev;

        return;
    }

    int counter = 1;
    while (metadata_info_buddy->is_free && order < MAX_ORDER) {


        validate_cookie(metadata_info_buddy);
        validate_cookie(metadata_info);

        void* address_metadata_first_buddy = (size_t)address_metadata < (size_t)address_metadata_buddy ?
                                             address_metadata : address_metadata_buddy;
        void* address_metadata_second_buddy = (size_t)address_metadata < (size_t)address_metadata_buddy ?
                                              address_metadata_buddy : address_metadata;
        struct MallocMetadata* metadata_info_first_buddy = (MallocMetadata*)address_metadata_first_buddy;
        struct MallocMetadata* metadata_info_second_buddy = (MallocMetadata*)address_metadata_second_buddy;

        validate_cookie(metadata_info_first_buddy);
        metadata_info_first_buddy->size = 2 * metadata_info_first_buddy->size;
        metadata_info_first_buddy->is_free = true;


        //updating the allocations list
        validate_cookie(metadata_info_second_buddy);
        metadata_info_first_buddy->next = metadata_info_second_buddy->next;
        if(metadata_info_second_buddy->next) {
            validate_cookie(metadata_info_second_buddy->next);
            (metadata_info_second_buddy->next)->prev = metadata_info_first_buddy;
        }
        metadata_info_second_buddy->next = nullptr;
        metadata_info_second_buddy->prev = nullptr;


        //updating the free blocks list - old size
        validate_cookie(metadata_info_buddy->prev_free);
        validate_cookie(metadata_info_buddy->next_free);
        metadata_info_buddy->prev_free->next_free = metadata_info_buddy->next_free;
        if (metadata_info_buddy->next_free) {
            metadata_info_buddy->next_free->prev_free = metadata_info_buddy->prev_free;
        }
        metadata_info_buddy->next_free = nullptr;
        metadata_info_buddy->prev_free = nullptr;


        if (counter != 1) {
            metadata_info->prev_free->next_free = metadata_info->next_free;
            if (metadata_info->next_free) {
                metadata_info->next_free->prev_free = metadata_info->prev_free;
            }
            metadata_info->next_free = nullptr;
            metadata_info->prev_free = nullptr;
        }


        //calculate the entry int the free array of the new size
        int entry = calc_entry_for_free_array(metadata_info_first_buddy->size);
        if (entry == -1) {
            return;
        }


        //find the correct place for the new merged allocation in the new size free list
        MallocMetadata* metadata_free_array = free_array[entry].next_free;
        MallocMetadata* metadata_prev_free_array = &free_array[entry];


        while (metadata_free_array && metadata_free_array->address < metadata_info_first_buddy->address) {
            validate_cookie(metadata_free_array);
            metadata_prev_free_array = metadata_free_array;
            metadata_free_array = metadata_free_array->next_free;
        }


        if (!metadata_free_array) {
            metadata_info_first_buddy->prev_free = metadata_prev_free_array;
            metadata_prev_free_array->next_free = metadata_info_first_buddy;
            metadata_info_first_buddy->next_free = nullptr;
        }
        else {
            metadata_info_first_buddy->next_free = metadata_free_array;
            metadata_free_array->prev_free = metadata_info_first_buddy;

            metadata_info_first_buddy->prev_free = metadata_prev_free_array;
            metadata_prev_free_array->next_free = metadata_info_first_buddy;
        }


        address_metadata_buddy = (void*)((size_t)address_metadata_first_buddy ^
                                         metadata_info_first_buddy->size);
        metadata_info_buddy = (MallocMetadata*)address_metadata_buddy;

        address_metadata = address_metadata_first_buddy;
        metadata_info = (MallocMetadata*)address_metadata;

        order = calc_entry_for_free_array(metadata_info_buddy->size);

        counter++;


    }

}





//// ---4---
void* srealloc(void* oldp, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }
    if (!oldp) {
        return smalloc(size);
    }



    //find the address of old Metadata block
    void* old_address_metadata = (void*)((size_t)oldp - sizeof(MallocMetadata));
    struct MallocMetadata* block = (MallocMetadata*)old_address_metadata;


    validate_cookie(block);

    if(block->size >= size){
        return oldp;
    }

    if(size + sizeof (MallocMetadata) > 128*1024){
        validate_cookie(block);
        if(block->size == size + sizeof(MallocMetadata)){
            return oldp;
        }
        munmap(oldp, block->size);
        mmap_number_bytes -= (block->size - sizeof(MallocMetadata));

        void* address_mmap = mmap(NULL, size + sizeof(MallocMetadata),
                                  PROT_EXEC | PROT_WRITE | PROT_READ ,MAP_ANON | MAP_PRIVATE, -1, 0);
        if(address_mmap < 0){
            return nullptr;
        }
        mmap_number_bytes += size;
        return (void*)((size_t)address_mmap + sizeof (MallocMetadata));
    }


    // check buddies
    int num_merges = 0;
    MallocMetadata* metadata_buddy = (MallocMetadata*)((size_t)block ^ block->size);
    MallocMetadata* tmp_block = block;
    validate_cookie(metadata_buddy);
    size_t buddies_merged_size = metadata_buddy->size;
    bool is_mergable = false;

    while((metadata_buddy->is_free)){
        num_merges++;
        buddies_merged_size = buddies_merged_size*2;
        if(buddies_merged_size >= size + sizeof(MallocMetadata)){
            is_mergable = true;
            break;
        }
        tmp_block = (size_t)metadata_buddy < (size_t)tmp_block ?
                    metadata_buddy : tmp_block;
        metadata_buddy = (MallocMetadata*)((size_t)tmp_block ^ buddies_merged_size);
    }

    if(is_mergable){
        MallocMetadata* metadata_info_buddy = (MallocMetadata*)((size_t)block ^ block->size);
        tmp_block = block;
        for(int i = 0; i < num_merges; i++){

            validate_cookie(metadata_info_buddy);
            validate_cookie(tmp_block);

            void* address_metadata_first_buddy = (size_t)metadata_info_buddy < (size_t)tmp_block ?
                                                 (void*)metadata_info_buddy : (void*)tmp_block;
            void* address_metadata_second_buddy = (size_t)metadata_info_buddy < (size_t)tmp_block ?
                                                  (void*)tmp_block : (void*)metadata_info_buddy;
            struct MallocMetadata* metadata_info_first_buddy = (MallocMetadata*)address_metadata_first_buddy;
            struct MallocMetadata* metadata_info_second_buddy = (MallocMetadata*)address_metadata_second_buddy;

            validate_cookie(metadata_info_first_buddy);
            metadata_info_first_buddy->size = 2 * metadata_info_first_buddy->size;
            metadata_info_first_buddy->is_free = false;


            //updating the allocations list
            validate_cookie(metadata_info_second_buddy);
            metadata_info_first_buddy->next = metadata_info_second_buddy->next;
            if(metadata_info_second_buddy->next) {
                validate_cookie(metadata_info_second_buddy->next);
                (metadata_info_second_buddy->next)->prev = metadata_info_first_buddy;
            }
            metadata_info_second_buddy->next = nullptr;
            metadata_info_second_buddy->prev = nullptr;


            //updating the free blocks list - old size

            metadata_info_buddy->prev_free->next_free = metadata_info_buddy->next_free;
            if (metadata_info_buddy->next_free) {
                metadata_info_buddy->next_free->prev_free = metadata_info_buddy->prev_free;
            }
            metadata_info_buddy->next_free = nullptr;
            metadata_info_buddy->prev_free = nullptr;



            metadata_info_buddy = (MallocMetadata*)((size_t)address_metadata_first_buddy ^
                                                    metadata_info_first_buddy->size);
            tmp_block = (MallocMetadata*)address_metadata_first_buddy;
        }
        memmove((void*)((size_t)(tmp_block) + sizeof(MallocMetadata)), (void*)((size_t)(block) + sizeof(MallocMetadata)), block->size);
        return (void*)((size_t)(tmp_block) + sizeof(MallocMetadata));
    }
    else {

        void* new_address_block = smalloc(size);
        memmove(new_address_block, oldp, block->size);
        sfree(oldp);
        return new_address_block;
    }

}


//// ---5---
size_t _num_free_blocks(){

    size_t counter = 0;
    struct MallocMetadata *block = &dummy_head;

    // start from first block - not from dummy

    block = block->next;

    while(block){
        validate_cookie(block);
        if(block->is_free){
            counter++;
        }
        block = block->next;
    }

    return counter;
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

//// ---6---
size_t _num_free_bytes(){

    size_t free_bytes = 0;

    struct MallocMetadata *block = &dummy_head;
    block = block->next;

    // count free bytes without metadata
    while(block){
        validate_cookie(block);
        if(block->is_free){
            free_bytes += block->size;
        }
        block = block->next;
    }

    size_t free_blocks = _num_free_blocks();
    size_t size_metadata = _size_meta_data();
    return free_bytes - (free_blocks*size_metadata);

}


//// ---7---
size_t _num_allocated_blocks(){

    size_t counter = 0;
    struct MallocMetadata *block = &dummy_head;


    // start from first block - not from dummy
    block = block->next;

    while(block){
        validate_cookie(block);
        counter++;
        block = block->next;
    }

    return counter + mmap_number_blocks;

}



//// ---8---
size_t _num_allocated_bytes(){

    size_t allocated_bytes = 0;

    struct MallocMetadata *block = &dummy_head;
    block = block->next;

    // count free bytes without metadata
    while(block){

        validate_cookie(block);
        allocated_bytes += block->size;
        block = block->next;
    }

    size_t metadata_bytes = _num_meta_data_bytes();
    return allocated_bytes + mmap_number_bytes - metadata_bytes;
}




//// utils
struct MallocMetadata* find_block(size_t size){

    struct MallocMetadata* block = &dummy_head;
    block = block->next;

    if(!block){
        return nullptr;
    }

    while(block){

        validate_cookie(block);
        if(block->is_free && block->size >= size){
            return block;
        }
        block = block->next;
    }

    return nullptr;
}

int find_order(size_t size_with_metadata){

    for(int i = 0; i<MAX_ORDER+1 ; i++){
        if(i == 0){
            if((double)size_with_metadata <= pow(2,7)){
                return 0;
            }
        } else {
            if((double)size_with_metadata > pow(2,6+i) && (double)size_with_metadata <= pow(2, 7+i)){
                return i;
            }
        }
    }
    // in case size bigger than 128KB:
    return -1;
}

MallocMetadata* get_best_fit(int order, int* best_fit_order){

    for(int i = order; i < MAX_ORDER+1; i++) {
        if(free_array[i].next_free){
            *best_fit_order = i;
            return free_array[i].next_free;
        }
    }

    return nullptr;
}

void validate_cookie(MallocMetadata* metadata){

    if(!metadata){
        return;
    }
    if(metadata->cookie != rand_cookie){
        exit(0xdeadbeef);
    }
}

