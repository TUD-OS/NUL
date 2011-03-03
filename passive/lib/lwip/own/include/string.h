static inline void * memset(void *dst, int c, unsigned long count) {

  void *res = dst;
  unsigned value = (c & 0xff) * 0x01010101;
  if (count & 1) asm volatile ("stosb" : "+D"(dst) : "a"(value) : "memory");
  if (count & 2) asm volatile ("stosw" : "+D"(dst) : "a"(value) : "memory");
  count /= 4;
  asm volatile ("rep stosl" : "+D"(dst), "+c"(count) : "a"(value)  : "memory");
  return res;
}

static inline void *memcpy(void *dst, const void *src, unsigned long count) {

  void *res = dst;
  if (count & 1) asm volatile ("movsb" : "+D"(dst), "+S"(src) : : "memory");
  if (count & 2) asm volatile ("movsw" : "+D"(dst), "+S"(src) : : "memory");
  count /= 4;
  asm volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c" (count) : : "memory");
  return res;
}

static inline int memcmp(const void *dst, const void *src, unsigned long count) {
  const char *d = (const char *)(dst);
  const char *s = (const char *)(src);
  unsigned diff = 0;
  while (!diff && count--) diff = *d++ - *s++;
  return diff;
}

static inline unsigned long strnlen(const char *src, unsigned long maxlen) {
  unsigned i=0;
  while (src[i] && maxlen--) i++;
  return i;
}


static inline unsigned long strlen(const char *src) { return strnlen(src, ~0ul); }
