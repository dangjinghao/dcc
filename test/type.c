#include <assert.h>

int main(){
    assert((sizeof(~(char)1) == sizeof(int)));
    assert((sizeof(~(long)1) == sizeof(long)));    
    assert((sizeof(~(char)1 >> 1) == sizeof(int)));    
    assert((sizeof(~(long)1 >> 1) == sizeof(long)));    
}