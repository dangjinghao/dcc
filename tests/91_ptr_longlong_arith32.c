int printf(const char *, ...);

int main(void)
{
    // dcc modified this statement because of the weak array and static inference ability
    char t[10] = "012345678"; 
    char *data = t;
    unsigned long long r = 4;
    unsigned a = 5;
    unsigned long long b = 12;

    *(unsigned*)(data + r) += a - b;

    printf("data = \"%s\"\n", data);
    return 0;
}
