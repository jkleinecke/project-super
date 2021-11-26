int _fltused = 0x9875;

#pragma function(memset)
void *memset(void *_Dst, int _Val, size_t _Size)
{
    unsigned char Val = *(unsigned char *)&_Val;
    unsigned char *At = (unsigned char *)_Dst;
    while(_Size--)
    {
        *At++ = Val;
    }
    return(_Dst);
}

#pragma function(memcpy)
void *memcpy(void *dest, void *src, size_t size)
{
    size_t chunks = size / sizeof(src); // Copy by cpu bit-width
    size_t slice = size % sizeof(src);  // remaining bytes < CPU bit width

    unsigned long long* sptr = (unsigned long long*)src;
    unsigned long long* dptr = (unsigned long long*)dest;

    while(chunks--) {
        *dptr++ = *sptr++;
    }

    unsigned char* s8 = (unsigned char*)sptr;
    unsigned char* d8 = (unsigned char*)dptr;

    while(slice--) {
        *d8++ = *s8++;
    }

    return dest;
}

// #pragma function(tanf)
// float tanf(float p) {
//     return 0.0f;
// }