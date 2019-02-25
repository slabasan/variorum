#include <stdio.h>

#include <variorum.h>

int main(int argc, char **argv)
{
    int ret;

    ret = dump_available_frequencies();
    if (ret != 0)
    {
        printf("Dump available frequencies failed!\n");
    }
    return ret;
}
