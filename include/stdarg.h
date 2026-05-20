#ifndef __STDARG_H
#define __STDARG_H

#if defined(__x86_64__) && !defined(_WIN64)
// Linux/macOS x86_64 SysV ABI
typedef struct {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} __va_list_tag;

static void *__va_arg_mem(__va_list_tag *ap, int sz, int align) {
  void *p = ap->overflow_arg_area;
  if (align > 8)
    p = (void *)(((unsigned long)p + 15) / 16 * 16);
  ap->overflow_arg_area = (void *)(((unsigned long)p + sz + 7) / 8 * 8);
  return p;
}

static void *__va_arg_gp(__va_list_tag *ap, int sz, int align) {
  if (ap->gp_offset >= 48)
    return __va_arg_mem(ap, sz, align);

  void *r = ap->reg_save_area + ap->gp_offset;
  ap->gp_offset += 8;
  return r;
}

static void *__va_arg_fp(__va_list_tag *ap, int sz, int align) {
  if (ap->fp_offset >= 176)
    return __va_arg_mem(ap, sz, align);

  void *r = ap->reg_save_area + ap->fp_offset;
  ap->fp_offset += 16;
  return r;
}

// It's a little hard to implement va_arg in LLVM backend so now we just
// implement it in C
#define va_arg(ap, ty)                                                         \
  ({                                                                           \
    int klass = __builtin_reg_class(ty);                                       \
    *(ty *)(klass == 0   ? __va_arg_gp(ap, sizeof(ty), _Alignof(ty))           \
            : klass == 1 ? __va_arg_fp(ap, sizeof(ty), _Alignof(ty))           \
                         : __va_arg_mem(ap, sizeof(ty), _Alignof(ty)));        \
  })

#endif

typedef __va_list_tag va_list[1];

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#define __GNUC_VA_LIST 1
typedef va_list __gnuc_va_list;
#endif