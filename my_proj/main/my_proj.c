#include <stdio.h>
#include <unistd.h>

void app_main(void)
{
    while (1) {
        printf("Hello World!\n");
        sleep(1);
    }
}