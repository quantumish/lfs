#include <stdio.h>
#include <fcntl.h>

int main() {
    return creat("mountdir/huh", 0644);    
}
