#include <stdio.h>

int fred(int p)
{
   printf("yo %d\n", p);
   return 42;
}


/* To test what this is supposed to test the destination function
   (fprint here) must not be called directly anywhere in the test.  */

int main()
{
   // dcc modified this statement because of the weak static inference ability and incomplete struct type support
   int (*f)(int) = &fred;
   int (*printfptr)(const char *, ...) = &printf;
   printfptr("%d\n", (*f)(24));

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
