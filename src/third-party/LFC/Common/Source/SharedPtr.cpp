/*
 |--------------------------------------------------------------------------
 | ԭ�������Լ�
 |--------------------------------------------------------------------------
 |
 | �� *pw Ϊ 0, �򲻸ı���ֵ������ 0, �� *pw ������ 0, ���Լ�һ�β�����ԭֵ
 |
 */
int atomic_conditional_increment(int *pw)
{
    // int rv = *pw;
    // if( rv != 0 ) ++*pw;
    // return rv;
    int rv, tmp;

    __asm__
        (
            "movl %0, %%eax\n\t"
            "0:\n\t"
            "test %%eax, %%eax\n\t"
            "je 1f\n\t"
            "movl %%eax, %2\n\t"
            "incl %2\n\t"
            "lock\n\t"
            "cmpxchgl %2, %0\n\t"
            "jne 0b\n\t"
            "1:":
    "=m"(*pw), "=&a"(rv), "=&r"(tmp) : // outputs (%0, %1, %2)
        "m"(*pw) :                         // input (%3)
        "cc"                              // clobbers
        );

    return rv;
}

void EnableSharedFromThis(...)
{
}
