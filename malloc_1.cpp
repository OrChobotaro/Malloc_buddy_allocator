#include <cmath>
#include <unistd.h>


void* smalloc(size_t size){

    if(size == 0){
        return nullptr;
    }
    if(size > pow(10, 8)){
        return nullptr;
    }
    void* first_byte = sbrk(size);
    if(first_byte == (void*)-1){
        return nullptr;
    }

    return first_byte;

}


