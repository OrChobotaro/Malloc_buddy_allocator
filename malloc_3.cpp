#include <unistd.h>
#include <cmath>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
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

MallocMetadata free_array[MAX_ORDER+1];
MallocMetadata arr[32];
bool is_first_malloc = true;

struct MallocMetadata dummy_head = {
        0, false, nullptr, nullptr,nullptr
};



//// functions declarations:
struct MallocMetadata* find_block(size_t size);
int find_order(size_t size_with_metadata);
MallocMetadata* get_best_fit(int order, int* best_fit_order);
size_t _num_free_blocks();
size_t _num_allocated_blocks();



//// ---1---
void* smalloc(size_t size){


    if(size == 0){
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }

    if(is_first_malloc){
        is_first_malloc = false;
        // align allocation
        void* curr_program_break = sbrk(0);
        size_t reminder = (size_t)curr_program_break % (32*128*1024);
        size_t difference = 32*128*1024 - reminder;
        void* address_of_initial_allocation = sbrk(32*128*1024 + difference);


        void* address_of_aligned_allocation = (void*)((size_t)address_of_initial_allocation + difference);

        // save metadata to memory blocks
        for(int i = 0; i<32; i++){
            /*struct MallocMetadata node = {
                    128*1024, true, nullptr, nullptr,nullptr,
                    nullptr, (void*)((size_t)address_of_initial_allocation + i*128*1024)
            };
*/
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
                    0, false, nullptr, nullptr,nullptr,
                    nullptr, nullptr
            };

            free_array[i] = dummy_node;
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


    size_t size_with_metadata = size + sizeof (MallocMetadata);

    // find the order of wanted block
    int order = find_order(size_with_metadata);

//    cout << "size entered: " << size << ", size with metadata: " << size_with_metadata;

    if(order < 0){
        void* address_mmap = mmap(NULL, size_with_metadata, PROT_EXEC | PROT_WRITE | PROT_READ ,MAP_ANON | MAP_PRIVATE, -1, 0);
        if(address_mmap < 0){
            return nullptr;
        }

//        cout << "after mmap " << address_mmap << endl;
        struct MallocMetadata mmap_block_metadata = {
                size_with_metadata, true, nullptr, nullptr, nullptr,
                nullptr, (void*)address_mmap
        };

        *(MallocMetadata*)address_mmap = mmap_block_metadata;
//        cout << "address of metadata: " << address_mmap << endl;
        return (void*)((size_t)address_mmap + sizeof(MallocMetadata));
    }

//    cout << " and order is: " << order << endl;


    int best_fit_order = 0;
    MallocMetadata* best_fit_metadata = get_best_fit(order, &best_fit_order);
    best_fit_metadata = free_array[best_fit_order].next_free;

    if(!best_fit_metadata){

//        cout << "hereee" << endl;
        return nullptr;
    }
//    cout << "best_fit_order : " << best_fit_order << endl;


    while(best_fit_order > order){
//        cout << "in loop" << endl;
        // create new metadata

//        cout << "--1--" << endl;

        struct MallocMetadata splitted_block_metadata = {
                best_fit_metadata->size/2, true, nullptr, nullptr,nullptr,
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
            best_fit_metadata->next->prev = (MallocMetadata*)splitted_block_metadata.address;
        }
        best_fit_metadata->next = (MallocMetadata*)splitted_block_metadata.address;
        splitted_block_metadata.prev = best_fit_metadata;




        // save new metadata to memory
        *(MallocMetadata*)(splitted_block_metadata.address) = splitted_block_metadata;

//        cout << "--7--" << endl;

        best_fit_metadata = get_best_fit(order, &best_fit_order);
//        cout << "best_fit_metadata->order : " << best_fit_order << endl;

    }


    best_fit_metadata->is_free = false;

    // remove best fit
    free_array[best_fit_order].next_free = best_fit_metadata->next_free;
    if(best_fit_metadata->next_free){
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
    MallocMetadata* address_metadata = (MallocMetadata*)((size_t)p - (size_t)sizeof(MallocMetadata));
    cout << "address_metadata->size is:" << address_metadata->size << endl;
    struct MallocMetadata* metadata_info = (MallocMetadata*)address_metadata;

    //if a block is already free - return
    if (metadata_info->is_free) {
        return;
    }

    if (metadata_info->size > 1024*128) {
        munmap(p, metadata_info->size);
        return;
    }

    void* address_metadata_buddy = (void*)((size_t)address_metadata ^ metadata_info->size);
    struct MallocMetadata* metadata_info_buddy = (MallocMetadata*)address_metadata_buddy;
    int order = calc_entry_for_free_array(metadata_info_buddy->size);

//    cout << "" << endl;
//    cout << "/////// begin free ////////" <<endl;
//    cout << "block to free: " << p <<"  and size is: " << metadata_info->size <<endl;

    if (!metadata_info_buddy->is_free || order == MAX_ORDER) {
//        cout << "block buddy is not free" <<endl;
        metadata_info->is_free = true;
        return;
    }


    while (metadata_info_buddy->is_free && order < MAX_ORDER) {

//        cout << "buddy size is: " << metadata_info_buddy->size << ", order: " << order << endl;
//        cout << "metadata address: " << metadata_info << ",     buddy address: " << metadata_info_buddy << endl;
//
//        cout << "---1----" << endl;


        void* address_metadata_first_buddy = (size_t)address_metadata < (size_t)address_metadata_buddy ?
                address_metadata : address_metadata_buddy;
        void* address_metadata_second_buddy = (size_t)address_metadata < (size_t)address_metadata_buddy ?
                address_metadata_buddy : address_metadata;
        struct MallocMetadata* metadata_info_first_buddy = (MallocMetadata*)address_metadata_first_buddy;
        struct MallocMetadata* metadata_info_second_buddy = (MallocMetadata*)address_metadata_second_buddy;

        //cout << "---3----" << endl;

        metadata_info_first_buddy->size = 2 * metadata_info_first_buddy->size;
        metadata_info_first_buddy->is_free = true;

        //cout << "---4----" << endl;

        //updating the allocations list
        metadata_info_first_buddy->next = metadata_info_second_buddy->next;
        if(metadata_info_second_buddy->next) {
            (metadata_info_second_buddy->next)->prev = metadata_info_first_buddy;
        }
        //cout << "SFREE:: num_blocks_free = " << _num_free_blocks() << endl;
        metadata_info_second_buddy->next = nullptr;
        metadata_info_second_buddy->prev = nullptr;

        //updating the free blocks list - old size
        metadata_info_buddy->prev_free = metadata_info_buddy->next_free;
        if (metadata_info_buddy->next_free) {
            metadata_info_buddy->next_free = metadata_info_buddy->prev_free;
        }
        metadata_info_buddy->next_free = nullptr;
        metadata_info_buddy->prev_free = nullptr;


        /*metadata_info_first_buddy->prev_free = metadata_info_second_buddy->next_free;
        if(metadata_info_second_buddy->next_free) {
            (metadata_info_second_buddy->next_free)->prev = metadata_info_first_buddy->prev_free;
        }
        metadata_info_second_buddy->next_free = nullptr;
        metadata_info_second_buddy->prev_free = nullptr;*/

        //cout << "FREE:: after updating old -  num_blocks_free = " << _num_free_blocks() << endl;

        //cout << "---5----" << endl;

 (metadata_info_second_buddy->prev_free)->next_free = metadata_info_second_buddy->next_free;
        (metadata_info_second_buddy->next_free)->prev_free = metadata_info_second_buddy->prev_free;
        metadata_info_second_buddy->next_free = nullptr;
        metadata_info_second_buddy->prev_free = nullptr;



        //calculate the entry int the free array of the new size
        int entry = calc_entry_for_free_array(metadata_info_first_buddy->size);
        if (entry == -1) {
            //////////error
            //assert(!(entry == -1));
            return;
        }


        //cout << "---6----" << endl;

        //find the correct place for the new merged allocation in the new size free list
        MallocMetadata* metadata_free_array = free_array[entry].next_free;
        MallocMetadata* metadata_prev_free_array = &free_array[entry];



        //cout << "new_entry " << entry << endl;

        while (metadata_free_array && metadata_free_array->address < metadata_info_first_buddy->address) {
            metadata_prev_free_array = metadata_free_array;
            metadata_free_array = metadata_free_array->next_free;
        }

        //cout << "---7----" << endl;

        if (!metadata_free_array) {
            metadata_info_first_buddy->prev_free = metadata_prev_free_array;
            metadata_prev_free_array->next_free = metadata_info_first_buddy;
            metadata_info_first_buddy->next_free = nullptr;
            //cout << "---8.1----" << endl;
        }
        else {
            metadata_info_first_buddy->next_free = metadata_free_array;
            metadata_free_array->prev_free = metadata_info_first_buddy;

            metadata_info_first_buddy->prev_free = metadata_prev_free_array;
            metadata_prev_free_array->next = metadata_info_first_buddy;
            //cout << "---8.2----" << endl;
        }

        //cout << "FREE:: after updating ***new*** -  num_blocks_free = " << _num_free_blocks() << endl;

        address_metadata_buddy = (void*)((size_t)address_metadata_first_buddy ^
                metadata_info_first_buddy->size);
        metadata_info_buddy = (MallocMetadata*)address_metadata_buddy;

        address_metadata = address_metadata_first_buddy;

        order = calc_entry_for_free_array(metadata_info_buddy->size);

        //cout << "---9----" << endl;

//        cout << "--------------SFREE:: num_blocks_free = " << _num_free_blocks() << endl;
//        cout << "--------------SFREE:: num_allocated = " << _num_allocated_blocks() << endl;
        cout << "" << endl;

    }
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
//        cout << "--in while loop--" << endl;
//        cout << "block size: " << block->size << endl;
        counter++;
        block = block->next;
    }


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


//// utils
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
//            cout << "size is" << free_array[i].next_free->size << endl;
            *best_fit_order = i;
            return free_array[i].next_free;
        }
    }

    return nullptr;
}





int main(){

/*    smalloc(10);
    smalloc(100);*/

//    smalloc(1000);
    void* address1 = smalloc(10000);
    void* address2 = smalloc(10000);
    void* address3 = smalloc(40000);
    //smalloc(10000);

    cout << "---finished allocation---" << endl;
    int num_allocated = _num_allocated_blocks();
    cout << "---num allocated blocks---" << endl;
    int num_bytes = _num_allocated_bytes();
    cout << "---num allocated bytes---" << endl;
    int num_free = _num_free_blocks();
    int num_free_bytes = _num_free_bytes();

    cout << "num allocated blocks: " << num_allocated << endl;
    cout << "num allocated bytes: " << num_bytes << endl;
    cout << "num free blocks: " << num_free << endl;
    cout << "num free bytes: " << num_free_bytes << endl;

    cout << "--------------------------------" <<endl;
    cout << "" <<endl;

    sfree(address2);

    cout << "---finished allocation---" << endl;
    num_allocated = _num_allocated_blocks();
    cout << "---num allocated blocks---" << endl;
    num_bytes = _num_allocated_bytes();
    cout << "---num allocated bytes---" << endl;
    num_free = _num_free_blocks();
    num_free_bytes = _num_free_bytes();

    cout << "num allocated blocks: " << num_allocated << endl;
    cout << "num allocated bytes: " << num_bytes << endl;
    cout << "num free blocks: " << num_free << endl;
    cout << "num free bytes: " << num_free_bytes << endl;
    cout << "--------------------------------" <<endl;
    cout << "" <<endl;

    sfree(address3);

    cout << "---finished allocation---" << endl;
    num_allocated = _num_allocated_blocks();
    cout << "---num allocated blocks---" << endl;
    num_bytes = _num_allocated_bytes();
    cout << "---num allocated bytes---" << endl;
    num_free = _num_free_blocks();
    num_free_bytes = _num_free_bytes();

    cout << "num allocated blocks: " << num_allocated << endl;
    cout << "num allocated bytes: " << num_bytes << endl;
    cout << "num free blocks: " << num_free << endl;
    cout << "num free bytes: " << num_free_bytes << endl;
    cout << "--------------------------------" <<endl;
    cout << "" <<endl;

    sfree(address1);


//    smalloc(1000000);
//    sfree(address);


/*    assert(free_array[MAX_ORDER].next_free);
    assert(!free_array[9].next_free->next_free);
    assert(!free_array[8].next_free->next_free);
    assert(!free_array[7].next_free);*/


    cout << "---finished allocation---" << endl;
    num_allocated = _num_allocated_blocks();
    cout << "---num allocated blocks---" << endl;
    num_bytes = _num_allocated_bytes();
    cout << "---num allocated bytes---" << endl;
    num_free = _num_free_blocks();
    num_free_bytes = _num_free_bytes();


    cout << "num allocated blocks: " << num_allocated << endl;
    cout << "num allocated bytes: " << num_bytes << endl;
    cout << "num free blocks: " << num_free << endl;
    cout << "num free bytes: " << num_free_bytes << endl;





    return 0;
}


