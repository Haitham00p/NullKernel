#include <framebuffer.h>
#define true 1
#define false 0

int main(){
    int i = 0;
    while(true){
        FrDrawLine32(i, 100 - i, i, i, 0xFFFFFFFF);
        i = i + 1;
    }
    return 0;
}