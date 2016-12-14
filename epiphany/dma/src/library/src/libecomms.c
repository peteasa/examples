#include <config.h>
#include <stdio.h>
#include <libecomms.h>

int soso(int inp)
{
    printf("soso(%d): This is " PACKAGE_STRING "\n", inp);

    return inp * 2;
}
