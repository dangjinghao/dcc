
#include "dcc.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LLVMContextRef C;
static LLVMModuleRef M;
static LLVMBuilderRef B;
static LLVMValueRef F;

static HashMap block_labels;
static HashMap func_labels_as_values;

static LLVMValueRef cmp_nz(LLVMValueRef v);
static LLVMValueRef cmp_ez(LLVMValueRef v);
static LLVMValueRef load(Type *ty, LLVMValueRef ptr);
static void store(Type *ty, LLVMValueRef ptr, LLVMValueRef v);

static void gen_switch_cmp_algo_rev_direct(Node *node,
                                           LLVMBasicBlockRef bb_after);

static void llvm_memset2(LLVMValueRef ptr, LLVMValueRef byte, LLVMValueRef n,
                         LLVMValueRef is_volatile) {

  static LLVMValueRef declare;
  static LLVMTypeRef declare_ty;
  if (!declare) {
    LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0),
                i64 = LLVMInt64TypeInContext(C), i1 = LLVMInt1TypeInContext(C),
                i8 = LLVMInt8TypeInContext(C),
                void_ty = LLVMVoidTypeInContext(C);
    LLVMTypeRef memset_param_tys[] = {ptr, i8, i64, i1};

    declare_ty = LLVMFunctionType(void_ty, memset_param_tys, 4, false);

    declare = LLVMAddFunction(M, "llvm.memset.p0.i64", declare_ty);
  }
  LLVMBuildCall2(B, declare_ty, declare,
                 (LLVMValueRef[4]){ptr, byte, n, is_volatile}, 4, "");
}

static void llvm_memset(LLVMValueRef ptr, char byte, size_t n,
                        bool is_volatile) {
  llvm_memset2(ptr, LLVMConstInt(LLVMInt8TypeInContext(C), byte, false),
               LLVMConstInt(LLVMInt64TypeInContext(C), n, false),
               LLVMConstInt(LLVMInt1TypeInContext(C), is_volatile, false));
}

static void llvm_memcpy2(LLVMValueRef dest, LLVMValueRef src, LLVMValueRef n,
                         LLVMValueRef is_volatile) {
  static LLVMValueRef declare;
  static LLVMTypeRef declare_ty;
  if (!declare) {
    // declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
    LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0),
                i64 = LLVMInt64TypeInContext(C), i1 = LLVMInt1TypeInContext(C),
                void_ty = LLVMVoidTypeInContext(C);
    LLVMTypeRef memcpy_param_tys[] = {ptr, ptr, i64, i1};

    declare_ty = LLVMFunctionType(void_ty, memcpy_param_tys, 4, false);
    declare = LLVMAddFunction(M, "llvm.memcpy.p0.p0.i64", declare_ty);
  }
  LLVMBuildCall2(B, declare_ty, declare,
                 (LLVMValueRef[4]){dest, src, n, is_volatile}, 4, "");
}

static void llvm_memcpy(LLVMValueRef dest, LLVMValueRef src, size_t n,
                        bool is_volatile) {
  llvm_memcpy2(dest, src, LLVMConstInt(LLVMInt64TypeInContext(C), n, false),
               LLVMConstInt(LLVMInt1TypeInContext(C), is_volatile, false));
}

static void llvm_va_start(LLVMValueRef ptr) {
  static LLVMValueRef declare;
  static LLVMTypeRef declare_ty;
  if (!declare) {
    // declare void @llvm.va_start.p0(ptr)
    LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0),
                void_ty = LLVMVoidTypeInContext(C);
    declare_ty = LLVMFunctionType(void_ty, &ptr, 1, false);
    declare = LLVMAddFunction(M, "llvm.va_start.p0", declare_ty);
  }
  LLVMBuildCall2(B, declare_ty, declare, &ptr, 1, "");
}

static void llvm_va_end(LLVMValueRef ptr) {
  static LLVMValueRef declare;
  static LLVMTypeRef declare_ty;
  if (!declare) {
    // declare void @llvm.va_end.p0(ptr)
    LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0),
                void_ty = LLVMVoidTypeInContext(C);
    declare_ty = LLVMFunctionType(void_ty, &ptr, 1, false);
    declare = LLVMAddFunction(M, "llvm.va_end.p0", declare_ty);
  }
  LLVMBuildCall2(B, declare_ty, declare, &ptr, 1, "");
}

static void llvm_va_copy(LLVMValueRef dest, LLVMValueRef src) {
  static LLVMValueRef declare;
  static LLVMTypeRef declare_ty;
  if (!declare) {
    // declare void @llvm.va_copy.p0(ptr)
    LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0),
                void_ty = LLVMVoidTypeInContext(C);
    declare_ty = LLVMFunctionType(void_ty, (LLVMTypeRef[]){ptr, ptr}, 2, false);
    declare = LLVMAddFunction(M, "llvm.va_copy.p0", declare_ty);
  }
  LLVMBuildCall2(B, declare_ty, declare, (LLVMValueRef[]){dest, src}, 2, "");
}

static LLVMValueRef gen_expr(Node *node);
static LLVMValueRef gen_stmt(Node *node);

// Returns true if the struct type has any bitfield member.
static bool is_struct_bitfield(Type *ty) {
  for (Member *m = ty->members; m; m = m->next)
    if (m->is_bitfield)
      return true;
  return false;
}

static char *get_var_real_name(Obj *var) { return var->asm_label ?: var->name; }

static LLVMTypeRef type_convert(Type *ty) {
  switch (ty->kind) {
  case TY_VOID:
    return LLVMVoidTypeInContext(C);
  case TY_BOOL:
    return LLVMInt8TypeInContext(C);
  case TY_CHAR:
    return LLVMInt8TypeInContext(C);
  case TY_SHORT:
    return LLVMInt16TypeInContext(C);
  case TY_INT:
  case TY_ENUM:
    return LLVMInt32TypeInContext(C);
  case TY_LONG:
    return LLVMInt64TypeInContext(C);
  case TY_FLOAT:
    return LLVMFloatTypeInContext(C);
  case TY_DOUBLE:
    return LLVMDoubleTypeInContext(C);
  case TY_LDOUBLE:
    return LLVMX86FP80TypeInContext(C);
  case TY_PTR:
    return LLVMPointerTypeInContext(C, 0);
  case TY_ARRAY: {
    int repr_array_len = MAX(0, ty->array_len);
    return LLVMArrayType2(type_convert(ty->base), repr_array_len);
  }
  case TY_STRUCT: {
    if (is_struct_bitfield(ty)) {
      // Bitfield structs may have overlapping storage units that LLVM
      // typed structs cannot express. Decay to a byte array.
      LLVMTypeRef i8_arr = LLVMArrayType2(LLVMInt8TypeInContext(C), ty->size);
      return LLVMStructTypeInContext(C, &i8_arr, 1, ty->is_packed);
    }

    size_t member_count = next_iter_count(ty->members);
    LLVMTypeRef *members_type = calloc(member_count, sizeof(LLVMTypeRef));
    {
      size_t members_type_idx = 0;
      for (Member *m = ty->members; m; m = m->next) {
        if (m->align != m->ty->align) {
          todo_impl("aligned member in struct");
        }
        members_type[members_type_idx++] = type_convert(m->ty);
      }
      assert(member_count == members_type_idx);
    }
    LLVMTypeRef r =
        LLVMStructTypeInContext(C, members_type, member_count, ty->is_packed);
    free(members_type);
    return r;
  }
  case TY_FUNC: {
    size_t param_count = next_iter_count(ty->params);
    if (is_large_agg_type(ty->return_ty)) {
      param_count++;
    }
    LLVMTypeRef *params = calloc(param_count, sizeof(LLVMTypeRef));
    {
      size_t params_idx = 0;
      if (is_large_agg_type(ty->return_ty)) {
        params[params_idx++] = type_convert(pointer_to(ty->return_ty));
      }
      for (Type *p = ty->params; p; p = p->next) {
        if (is_agg_type(p)) {
          // large struct type -> struct pointer type
          if (is_large_agg_type(p)) {
            params[params_idx++] = type_convert(pointer_to(p));
            continue;
          }
          // else: for small struct type, treat them as normal type, LLVM
          // basically support this
        }
        params[params_idx++] = type_convert(p);
      }
      assert(params_idx == param_count);
    }

    LLVMTypeRef ret_ty = is_large_agg_type(ty->return_ty)
                             ? LLVMVoidTypeInContext(C)
                             : type_convert(ty->return_ty);

    LLVMTypeRef r =
        LLVMFunctionType(ret_ty, params, param_count, ty->is_variadic);
    free(params);
    return r;
  }
  case TY_UNION: {
    // find max type member and init union as this type
    Type *max_ty = ty->members->ty;
    for (Member *mem = ty->members; mem; mem = mem->next) {
      if (max_ty->size < mem->ty->size)
        max_ty = mem->ty;
    }
    return type_convert(max_ty);
  }
  case TY_VLA:
    return type_convert(pointer_to(ty->base));
  default:
    break;
  }
  unreachable();
}

// Write an integer value into a byte buffer at the given offset,
// using the size-dependent width for correct layout.
static void write_buf(char *buf, uint64_t val, int sz) {
  if (sz == 1)
    *buf = val;
  else if (sz == 2)
    *(uint16_t *)buf = val;
  else if (sz == 4)
    *(uint32_t *)buf = val;
  else if (sz == 8)
    *(uint64_t *)buf = val;
  else
    unreachable();
}

// Read an integer from a byte buffer with the given size.
static uint64_t read_buf(char *buf, int sz) {
  if (sz == 1)
    return *(uint8_t *)buf;
  else if (sz == 2)
    return *(uint16_t *)buf;
  else if (sz == 4)
    return *(uint32_t *)buf;
  else if (sz == 8)
    return *(uint64_t *)buf;
  else
    unreachable();
}

// Fill a byte buffer with the initial values for a bitfield-containing
// struct. DCC's flat-bit-counter layout may produce overlapping storage
// units, so we build a raw byte-level representation instead of typed
// LLVM struct members.
//
// Members whose initializers require relocations (addresses of functions
// or global variables) are skipped - they will be handled separately as
// native LLVM constants by init_bitfield_struct_global.
static void fill_bitfield_buf(Type *ty, Initializer *init, uint8_t *buf) {
  for (Member *m = ty->members; m; m = m->next) {
    Initializer *child = init->children[m->idx];
    if (!child)
      continue;

    if (m->is_bitfield) {
      if (!child->expr)
        continue;
      int64_t val = eval(child->expr);
      int sz = m->ty->size;
      uint64_t mask = (1ULL << m->bit_width) - 1;
      uint64_t bits = ((uint64_t)val & mask) << m->bit_offset;

      uint64_t prev = read_buf((char *)(buf + m->offset), sz);
      write_buf((char *)(buf + m->offset), prev | bits, sz);
    } else if (is_agg_type(m->ty) && !child->expr) {
      fill_bitfield_buf(m->ty, child, buf + m->offset);
    } else if (child->expr) {
      Node *var_node = NULL;
      int64_t val = eval2(child->expr, &var_node);
      if (var_node) {
        // Relocation member - skip, handled separately
        continue;
      }
      write_buf((char *)(buf + m->offset), (uint64_t)val, m->ty->size);
    }
  }
}

// Generate a relocation initializer for a scalar (non-bitfield) member.
// Returns NULL if the member does not require a relocation.
static LLVMValueRef gen_scalar_reloc_init(Type *ty, Initializer *init) {
  if (!init->expr)
    return NULL;

  if (ty->kind == TY_DOUBLE || ty->kind == TY_FLOAT)
    return NULL;

  Node *var_node = NULL;
  int64_t eval_val = eval2(init->expr, &var_node);
  if (!var_node)
    return NULL;

  if (var_node->kind == ND_LABEL_VAL) {
    LLVMValueRef target_fn = (LLVMValueRef)var_node->parent_fn->codegen_data;
    assert(target_fn);
    LLVMBasicBlockRef bb = hashmap_get(&block_labels, var_node->unique_label);
    if (!bb) {
      bb = LLVMAppendBasicBlockInContext(C, target_fn, var_node->unique_label);
      hashmap_put(&block_labels, var_node->unique_label, bb);
    }
    return LLVMBlockAddress(target_fn, bb);
  }

  assert(var_node->var);
  LLVMValueRef target_val = (LLVMValueRef)var_node->var->codegen_data;
  assert(target_val);
  assert(ty->base);
  LLVMValueRef byte_offset =
      LLVMConstInt(LLVMInt64TypeInContext(C), (uint64_t)eval_val, true);
  LLVMValueRef i8_ptr =
      LLVMConstGEP2(LLVMInt8TypeInContext(C), target_val, &byte_offset, 1);
  return LLVMConstBitCast(i8_ptr, LLVMPointerTypeInContext(C, 0));
}

// Build a global initializer for a bitfield-containing struct.
//
// Non-bitfield members whose initializers require relocations (addresses
// of functions/globals) are emitted as native LLVM constants (ptr, etc.)
// at their byte offset, while bitfields and non-relocation members form
// [k x i8] byte-array gaps. The result is a packed unnamed struct that
// matches the parser-computed byte layout exactly.
typedef struct {
  int offset;
  int size;
  LLVMValueRef val;
} RelocEntry;

// Recursively collect relocation entries from a type tree, including
// nested aggregate members (structs/unions embedded within the outer
// bitfield struct). base_offset tracks the cumulative offset from the
// outermost struct.
static void collect_relocs_rec(Type *ty, Initializer *init, int base_offset,
                               PtrArray *relocs) {
  if (!ty->members)
    return;
  for (Member *m = ty->members; m; m = m->next) {
    Initializer *child = init->children[m->idx];
    if (!child || m->is_bitfield)
      continue;
    if (is_agg_type(m->ty) && !child->expr) {
      collect_relocs_rec(m->ty, child, base_offset + m->offset, relocs);
      continue;
    }
    LLVMValueRef val = gen_scalar_reloc_init(m->ty, child);
    if (val) {
      RelocEntry *e = calloc(1, sizeof(RelocEntry));
      *e = (RelocEntry){base_offset + m->offset, m->ty->size, val};
      ptrarray_push(relocs, e);
    }
  }
}

static LLVMValueRef init_bitfield_struct_global(Type *ty, Initializer *init) {
  int sz = ty->size;
  uint8_t *buf = calloc(sz, 1);
  fill_bitfield_buf(ty, init, buf);

  PtrArray reloc_list = {};
  collect_relocs_rec(ty, init, 0, &reloc_list);
  int n_reloc = reloc_list.len;

  // Build LLVM struct elements: [k x i8] gaps + relocation values
  int max_elems = n_reloc * 2 + 1;
  LLVMValueRef *elems = calloc(max_elems, sizeof(LLVMValueRef));
  int n_elems = 0;
  int pos = 0;

  for (int i = 0; i < n_reloc; i++) {
    RelocEntry *re = (RelocEntry *)reloc_list.data[i];
    if (re->offset > pos) {
      int gap = re->offset - pos;
      LLVMValueRef *gap_bytes = calloc(gap, sizeof(LLVMValueRef));
      for (int j = 0; j < gap; j++) {
        gap_bytes[j] =
            LLVMConstInt(LLVMInt8TypeInContext(C), buf[pos + j], false);
      }
      elems[n_elems++] =
          LLVMConstArray(LLVMInt8TypeInContext(C), gap_bytes, gap);
      free(gap_bytes);
    }
    elems[n_elems++] = re->val;
    pos = re->offset + re->size;
  }

  if (pos < sz) {
    int gap = sz - pos;
    LLVMValueRef *gap_bytes = calloc(gap, sizeof(LLVMValueRef));
    for (int j = 0; j < gap; j++) {
      gap_bytes[j] =
          LLVMConstInt(LLVMInt8TypeInContext(C), buf[pos + j], false);
    }
    elems[n_elems++] = LLVMConstArray(LLVMInt8TypeInContext(C), gap_bytes, gap);
    free(gap_bytes);
  }

  bool packed = ty->is_packed || n_reloc > 0;
  LLVMValueRef r = LLVMConstStructInContext(C, elems, n_elems, packed);
  free(elems);
  for (int i = 0; i < n_reloc; i++)
    free(reloc_list.data[i]);
  ptrarray_free(&reloc_list);
  free(buf);
  return r;
}

static LLVMValueRef init_global_data(Type *ty, Initializer *init) {
  LLVMTypeRef llvm_ty = type_convert(ty);
  LLVMValueRef init_val;
  if (ty->kind == TY_ARRAY) {
    LLVMValueRef *cv_array = calloc(ty->array_len, sizeof(LLVMValueRef));
    for (int i = 0; i < ty->array_len; i++) {
      cv_array[i] = init_global_data(ty->base, init->children[i]);
    }
    init_val = LLVMConstArray2(type_convert(ty->base), cv_array, ty->array_len);
    free(cv_array);
  } else if (ty->kind == TY_UNION) {
    Member *init_mem = init->mem;
    if (init_mem) {
      // create specific union type
      LLVMValueRef val =
          init_global_data(init_mem->ty, init->children[init_mem->idx]);
      int padding = ty->size - init_mem->ty->size;
      if (padding > 0) {
        LLVMValueRef pad =
            LLVMConstNull(LLVMArrayType(LLVMInt8TypeInContext(C), padding));
        // unnamed struct value { init_val, <pad-array> }
        init_val =
            LLVMConstStructInContext(C, (LLVMValueRef[]){val, pad}, 2, false);
      } else {
        init_val = val;
      }
    } else {
      init_val = LLVMConstNull(type_convert(ty));
    }
  } else if (ty->kind == TY_STRUCT) {
    if (is_struct_bitfield(ty)) {
      init_val = init_bitfield_struct_global(ty, init);
    } else {
      size_t member_count = next_iter_count(ty->members);
      LLVMValueRef *cv_array = calloc(member_count, sizeof(LLVMValueRef));
      size_t cv_array_idx = 0;
      for (Member *m = ty->members; m; m = m->next)
        cv_array[cv_array_idx++] =
            init_global_data(m->ty, init->children[m->idx]);
      assert(cv_array_idx == member_count);
      init_val = LLVMConstNamedStruct(type_convert(ty), cv_array, member_count);
      free(cv_array);
    }
  } else if (!init->expr) {
    init_val = LLVMConstNull(llvm_ty);
  } else if (ty->kind == TY_DOUBLE || ty->kind == TY_FLOAT) {
    init_val = LLVMConstReal(llvm_ty, eval_double(init->expr));
  } else {
    // integer family and ptr
    Node *var_node = NULL;
    int64_t eval_val = eval2(init->expr, &var_node);
    if (var_node) {
      if (var_node->kind == ND_LABEL_VAL) {
        LLVMValueRef target_fn =
            (LLVMValueRef)var_node->parent_fn->codegen_data;
        assert(target_fn);
        LLVMBasicBlockRef bb =
            hashmap_get(&block_labels, var_node->unique_label);
        if (!bb) {
          bb = LLVMAppendBasicBlockInContext(C, target_fn,
                                             var_node->unique_label);
          hashmap_put(&block_labels, var_node->unique_label, bb);
        }
        init_val = LLVMBlockAddress(target_fn, bb);
      } else if (ty->kind == TY_PTR) {
        // pointer initialization with relocation
        assert(var_node->var);
        LLVMValueRef target_val = (LLVMValueRef)var_node->var->codegen_data;
        assert(target_val);
        LLVMValueRef byte_offset =
            LLVMConstInt(LLVMInt64TypeInContext(C), (uint64_t)eval_val, true);
        LLVMValueRef i8_ptr = LLVMConstGEP2(LLVMInt8TypeInContext(C),
                                            target_val, &byte_offset, 1);
        init_val = LLVMConstBitCast(i8_ptr, llvm_ty);
      } else {
        // cast pointer to integer initialization with relocation
        assert(var_node->var);
        LLVMValueRef target_val = (LLVMValueRef)var_node->var->codegen_data;
        assert(target_val);
        if (eval_val != 0) {
          LLVMValueRef byte_offset =
              LLVMConstInt(LLVMInt64TypeInContext(C), (uint64_t)eval_val, true);
          target_val = LLVMConstGEP2(LLVMInt8TypeInContext(C), target_val,
                                     &byte_offset, 1);
        }
        init_val = LLVMConstPtrToInt(target_val, type_convert(ty));
      }
    } else if (ty->kind == TY_PTR) {
      init_val =
          LLVMConstInt(LLVMInt64TypeInContext(C), eval_val, ty->is_unsigned);
      init_val = LLVMBuildIntToPtr(B, init_val, llvm_ty, "");
    } else {
      // int family
      init_val = LLVMConstInt(llvm_ty, eval_val, ty->is_unsigned);
    }
  }
  return init_val;
}

static void llvm_set_value_attr(Obj *o, LLVMValueRef v) {
  if (o->is_function) {
    if (o->is_inline) {
      unsigned int kind_id = LLVMGetEnumAttributeKindForName("inlinehint", 10);
      LLVMAttributeRef inline_attr = LLVMCreateEnumAttribute(C, kind_id, 0);
      LLVMAddAttributeAtIndex(v, LLVMAttributeFunctionIndex, inline_attr);
    }
  } else {
    LLVMSetAlignment(v, o->ty->align);
    if (opt_fcommon && o->is_tentative) {
      LLVMSetLinkage(v, LLVMCommonLinkage);
    }
    if (o->is_tls) {
      LLVMSetThreadLocal(v, true);
    }
  }

  if (o->is_static) {
    LLVMSetLinkage(v, LLVMInternalLinkage);
  }
  if (!o->is_definition) {
    LLVMSetLinkage(v, LLVMExternalLinkage);
  }
}

// create br instruction if current block is not terminataed. It seems that if
// there are more than one br in same block, it will be an ub situation
static void llvm_build_terminator_br(LLVMBasicBlockRef dest) {
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B))) {
    LLVMBuildBr(B, dest);
  }
}

static void new_block(char *name) {
  LLVMBasicBlockRef blk_name = LLVMAppendBasicBlockInContext(C, F, name);
  llvm_build_terminator_br(blk_name);
  LLVMPositionBuilderAtEnd(B, blk_name);
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F80, PTR };

static int get_type_id(Type *ty) {
  switch (ty->kind) {
  case TY_BOOL:
    return I8;
  case TY_CHAR:
    return ty->is_unsigned ? U8 : I8;
  case TY_SHORT:
    return ty->is_unsigned ? U16 : I16;
  case TY_INT:
    return ty->is_unsigned ? U32 : I32;
  case TY_ENUM:
    return I32;
  case TY_LONG:
    return ty->is_unsigned ? U64 : I64;
  case TY_FLOAT:
    return F32;
  case TY_DOUBLE:
    return F64;
  case TY_LDOUBLE:
    return F80;
  case TY_FUNC:
  case TY_ARRAY:
  case TY_PTR:
  case TY_VLA:
    return PTR;
  case TY_STRUCT:
  case TY_UNION:
    break;
  }
  unreachable();
}
enum { CAST_NOP = -1, CAST_INVALID = -2 };

static int cast_table[12][12] = {
    [I8] =
        {
            [I8] = CAST_NOP,
            [I16] = LLVMSExt,
            [I32] = LLVMSExt,
            [I64] = LLVMSExt,
            [U8] = CAST_NOP,
            [U16] = LLVMSExt,
            [U32] = LLVMSExt,
            [U64] = LLVMSExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F80] = LLVMSIToFP,
            [PTR] = LLVMIntToPtr,
        },
    [I16] =
        {
            [I8] = LLVMTrunc,
            [I16] = CAST_NOP,
            [I32] = LLVMSExt,
            [I64] = LLVMSExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = CAST_NOP,
            [U32] = LLVMSExt,
            [U64] = LLVMSExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F80] = LLVMSIToFP,
        },
    [I32] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = CAST_NOP,
            [I64] = LLVMSExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = CAST_NOP,
            [U64] = LLVMSExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F80] = LLVMSIToFP,
        },
    [I64] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = LLVMTrunc,
            [I64] = CAST_NOP,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = LLVMTrunc,
            [U64] = CAST_NOP,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F80] = LLVMSIToFP,
        },
    [U8] =
        {
            [I8] = CAST_NOP,
            [I16] = LLVMZExt,
            [I32] = LLVMZExt,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = CAST_NOP,
            [U16] = LLVMZExt,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F80] = LLVMUIToFP,
        },
    [U16] =
        {
            [I8] = LLVMTrunc,
            [I16] = CAST_NOP,
            [I32] = LLVMZExt,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = CAST_NOP,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F80] = LLVMUIToFP,
        },
    [U32] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = CAST_NOP,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = CAST_NOP,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F80] = LLVMUIToFP,
        },
    [U64] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = LLVMTrunc,
            [I64] = CAST_NOP,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = LLVMTrunc,
            [U64] = CAST_NOP,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F80] = LLVMUIToFP,
        },
    [F32] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = CAST_NOP,
            [F64] = LLVMFPExt,
            [F80] = LLVMFPExt,
            [PTR] = CAST_INVALID,
        },
    [F64] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = LLVMFPTrunc,
            [F64] = CAST_NOP,
            [F80] = LLVMFPExt,
            [PTR] = CAST_INVALID,
        },
    [F80] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = LLVMFPTrunc,
            [F64] = LLVMFPTrunc,
            [F80] = CAST_NOP,
            [PTR] = CAST_INVALID,
        },
    [PTR] =
        {
            [I8] = LLVMPtrToInt,
            [I16] = LLVMPtrToInt,
            [I32] = LLVMPtrToInt,
            [I64] = LLVMPtrToInt,
            [U8] = LLVMPtrToInt,
            [U16] = LLVMPtrToInt,
            [U32] = LLVMPtrToInt,
            [U64] = LLVMPtrToInt,
            [PTR] = CAST_NOP,
            [F32] = CAST_INVALID,
            [F64] = CAST_INVALID,
            [F80] = CAST_INVALID,
        },
};

static LLVMValueRef cast(LLVMValueRef v, Type *from, Type *to, Token *tok) {
  if (to->kind == TY_VOID || is_agg_type(to)) {
    return v;
  }
  if (to->kind == TY_BOOL) {
    // special case: (bool)x -> (char)!!x
    v = cmp_nz(v);
    return LLVMBuildCast(B, LLVMZExt, v, type_convert(to), "cast_bool");
  }
  int from_id = get_type_id(from);
  int to_id = get_type_id(to);
  int op = cast_table[from_id][to_id];
  if (op == CAST_NOP) {
    return v;
  } else if (op == CAST_INVALID) {
    error_tok(tok, "Invalid type cast");
  }
  return LLVMBuildCast(B, (LLVMOpcode)op, v, type_convert(to), "cast");
}

// Generate a pointer-arithmetic index value extended to a 64-bit integer.
//
// LLVM's getelementptr sign-extends a narrow index operand to the pointer
// width.  For C this is wrong when the index type is an unsigned type
// narrower than the pointer (e.g. `unsigned char`): such a value must be
// zero-extended.  We therefore widen the index here according to its own
// signedness so the subsequent GEP sees a full-width value.
static LLVMValueRef gen_ptr_index(Node *idx) {
  LLVMValueRef v = gen_expr(idx);
  return LLVMBuildIntCast2(B, v, LLVMInt64TypeInContext(C),
                           !idx->ty->is_unsigned, "ptr_idx");
}

// Read a bitfield value from its storage unit. `ptr` points to the
// start of the storage unit byte (from GEP into the [N x i8] array).
static LLVMValueRef bf_load(Member *mem, LLVMValueRef ptr) {
  int sz = mem->ty->size;
  LLVMTypeRef ity = LLVMIntTypeInContext(C, sz * 8);

  LLVMValueRef unit = LLVMBuildLoad2(B, ity, ptr, "bf_unit");
  uint64_t bf_mask = (1ULL << mem->bit_width) - 1;

  // (unit >> bit_offset) & mask
  LLVMValueRef v = LLVMBuildLShr(
      B, unit, LLVMConstInt(ity, mem->bit_offset, false), "bf_shr");
  v = LLVMBuildAnd(B, v, LLVMConstInt(ity, bf_mask, false), "bf_mask");

  // Sign-extend if the declared type is signed
  if (!mem->ty->is_unsigned && mem->bit_width > 0) {
    int shift_amt = sz * 8 - mem->bit_width;
    v = LLVMBuildShl(B, v, LLVMConstInt(ity, shift_amt, false), "bf_sext_shl");
    v = LLVMBuildAShr(B, v, LLVMConstInt(ity, shift_amt, false),
                      "bf_sext_ashr");
  }
  return v;
}

// Store a value into a bitfield. `ptr` points to the start of the
// storage unit byte. Performs read-modify-write.
static void bf_store(Member *mem, LLVMValueRef ptr, LLVMValueRef val) {
  int sz = mem->ty->size;
  LLVMTypeRef ity = LLVMIntTypeInContext(C, sz * 8);
  uint64_t bf_mask = (1ULL << mem->bit_width) - 1;

  LLVMValueRef unit = LLVMBuildLoad2(B, ity, ptr, "bf_old");

  // Clear old bits: unit & ~(mask << bit_offset)
  LLVMValueRef cleared = LLVMBuildAnd(
      B, unit, LLVMConstInt(ity, ~(bf_mask << mem->bit_offset), false),
      "bf_clear");

  // Mask new value and shift into position
  LLVMValueRef masked =
      LLVMBuildAnd(B, val, LLVMConstInt(ity, bf_mask, false), "bf_val_mask");
  LLVMValueRef shifted = LLVMBuildShl(
      B, masked, LLVMConstInt(ity, mem->bit_offset, false), "bf_shl");

  LLVMValueRef merged = LLVMBuildOr(B, cleared, shifted, "bf_merge");
  LLVMBuildStore(B, merged, ptr);
}

static LLVMValueRef gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    // the variable ptr has been stored in codegen_data
    return (LLVMValueRef)node->var->codegen_data;
  case ND_DEREF:
    return gen_expr(node->lhs);
  case ND_COMMA:
    gen_expr(node->lhs);
    return gen_addr(node->rhs);
  case ND_MEMBER: {
    LLVMValueRef ptr = gen_addr(node->lhs);
    if (node->lhs->ty->kind == TY_UNION) {
      // union type doesn't need gep
      return ptr;
    }
    if (is_struct_bitfield(node->lhs->ty)) {
      // return the bitfield starts byte address
      LLVMValueRef idx =
          LLVMConstInt(LLVMInt32TypeInContext(C), node->member->offset, false);
      return LLVMBuildGEP2(B, LLVMInt8TypeInContext(C), ptr, &idx, 1, "");
    }
    return LLVMBuildStructGEP2(B, type_convert(node->lhs->ty), ptr,
                               node->member->idx, "mem_GEP");
  }
  case ND_FUNCALL:
    if (is_agg_type(node->ty)) {
      gen_expr(node);
      assert(node->ret_buffer);
      LLVMValueRef ptr = (LLVMValueRef)node->ret_buffer->codegen_data;
      assert(ptr);
      return ptr;
    }
    break;
  case ND_ASSIGN:
    if (is_agg_type(node->ty)) {
      gen_expr(node);
      return gen_addr(node->lhs);
    }
    break;
  case ND_COND:
    if (is_agg_type(node->ty)) {
      if (!is_large_agg_type(node->ty)) {
        LLVMValueRef val = gen_expr(node);
        LLVMValueRef tmp = LLVMBuildAlloca(B, type_convert(node->ty), "tmp");
        store(node->ty, tmp, val);
        return tmp;
      }
      // if it's large_agg_type, gen_expr will return ptr directly
      return gen_expr(node);
    }
    break;
  case ND_VLA_PTR:
    return (LLVMValueRef)node->var->codegen_data;
  }

  error_tok(node->tok, "not an lvalue");
}

// Return the ptr if pointee_ty is large agg or vla.
// Or load basic type/small agg type value from ptr
static LLVMValueRef load(Type *pointee_ty, LLVMValueRef ptr) {
  switch (pointee_ty->kind) {
  case TY_ARRAY:
  case TY_FUNC:
    unreachable();
  case TY_STRUCT:
  case TY_UNION:
  case TY_VLA:
    if (is_agg_type(pointee_ty) && !is_large_agg_type(pointee_ty)) {
      // load small agg type directly
      break;
    }
    // we can't load them so we just return the ptr
    return ptr;
  }

  LLVMValueRef load_v =
      LLVMBuildLoad2(B, type_convert(pointee_ty), ptr, "load");
  // LLVM atomic memory operation needs restrict alignment
  LLVMSetAlignment(load_v, pointee_ty->align);
  if (pointee_ty->is_atomic)
    LLVMSetOrdering(load_v, LLVMAtomicOrderingSequentiallyConsistent);
  return load_v;
}

static void store(Type *ty, LLVMValueRef ptr, LLVMValueRef v) {
  switch (ty->kind) {
  case TY_STRUCT:
  case TY_UNION: {
    if (is_large_agg_type(ty)) {
      // v is the agg ptr if it is large agg
      llvm_memcpy(ptr, v, ty->size, false);
      return;
    }
    // small agg type is already loaded in v by gen_expr
  }
  default:
    break;
  }
  LLVMValueRef store_v = LLVMBuildStore(B, v, ptr);
  // LLVM atomic memory operation needs restrict alignment
  LLVMSetAlignment(store_v, ty->align);
  if (ty->is_atomic)
    LLVMSetOrdering(store_v, LLVMAtomicOrderingSequentiallyConsistent);
}

// Return i1, so do not use it as _Bool(i8) type directly
static LLVMValueRef cmp_nz(LLVMValueRef v) {
  LLVMTypeRef vty = LLVMTypeOf(v);
  LLVMTypeKind vk = LLVMGetTypeKind(vty);
  LLVMValueRef zero = LLVMConstNull(vty);
  if (vk == LLVMIntegerTypeKind || vk == LLVMPointerTypeKind) {
    return LLVMBuildICmp(B, LLVMIntNE, v, zero, "cmp_nz_i");
  }
  return LLVMBuildFCmp(B, LLVMRealONE, v, zero, "cmp_nz_f");
}

// Return i1, so do not use it as _Bool(i8) type directly
static LLVMValueRef cmp_ez(LLVMValueRef v) {
  LLVMTypeRef vty = LLVMTypeOf(v);
  LLVMTypeKind vk = LLVMGetTypeKind(vty);
  LLVMValueRef zero = LLVMConstNull(vty);
  if (vk == LLVMIntegerTypeKind || vk == LLVMPointerTypeKind) {
    return LLVMBuildICmp(B, LLVMIntEQ, v, zero, "cmp_ez_i");
  }
  return LLVMBuildFCmp(B, LLVMRealOEQ, v, zero, "cmp_ez_f");
}

static LLVMValueRef logic_short_circuit(Node *node, bool is_and) {
  new_block("logic_short_circuit");
  LLVMValueRef lhs = gen_expr(node->lhs);
  LLVMValueRef lhs_check = cmp_nz(lhs);
  LLVMBasicBlockRef start_block = LLVMGetInsertBlock(B);
  LLVMBasicBlockRef next_block =
      LLVMAppendBasicBlockInContext(C, F, is_and ? "and_next" : "or_next");
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(C, F, is_and ? "and_merge" : "or_merge");

  if (is_and) {
    LLVMBuildCondBr(B, lhs_check, next_block, merge_block);
  } else {
    LLVMBuildCondBr(B, lhs_check, merge_block, next_block);
  }

  LLVMPositionBuilderAtEnd(B, next_block);
  LLVMValueRef rhs = gen_expr(node->rhs);
  LLVMValueRef rhs_check = cmp_nz(rhs);
  // update then block which maybe updated by sub-expression
  next_block = LLVMGetInsertBlock(B);
  llvm_build_terminator_br(merge_block);

  LLVMPositionBuilderAtEnd(B, merge_block);
  LLVMValueRef phi =
      LLVMBuildPhi(B, LLVMInt1TypeInContext(C), is_and ? "and_phi" : "or_phi");
  LLVMAddIncoming(phi, (LLVMValueRef[]){lhs_check, rhs_check},
                  (LLVMBasicBlockRef[]){start_block, next_block}, 2);
  LLVMValueRef ext = LLVMBuildZExt(B, phi, LLVMInt32TypeInContext(C),
                                   is_and ? "zext_and" : "zext_or");
  return ext;
}

static LLVMValueRef build_bitcast_to_integer_type(Type *ty, LLVMValueRef v) {
  if (is_flonum(ty)) {
    if (ty->kind == TY_FLOAT) {
      return LLVMBuildBitCast(B, v, type_convert(ty_int), "bitcast_float_int");
    }
    if (ty->kind == TY_DOUBLE) {
      return LLVMBuildBitCast(B, v, type_convert(ty_long),
                              "bitcast_double_long");
    }
  }
  return v;
}

// ({
//   do {
//    new = *ptr <op> rhs;
//   } while (!cas(ptr, old, new));
//   new;
// })
static LLVMValueRef build_sa_atomic_rmw(LLVMOpcode kind, Type *ty,
                                        LLVMValueRef ptr, LLVMValueRef rhs) {

  new_block("atomic_rmw");
  LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(C, F, "armw_body");
  LLVMBasicBlockRef bb_cond = LLVMAppendBasicBlockInContext(C, F, "armw_cond");
  LLVMBasicBlockRef bb_merge =
      LLVMAppendBasicBlockInContext(C, F, "armw_merge");
  llvm_build_terminator_br(bb_body);
  LLVMPositionBuilderAtEnd(B, bb_body);
  // new = *ptr <op> rhs;
  LLVMValueRef old_v = load(ty, ptr);
  LLVMValueRef new_v = LLVMBuildBinOp(B, kind, old_v, rhs, "armw_op");
  llvm_build_terminator_br(bb_cond);
  LLVMPositionBuilderAtEnd(B, bb_cond);
  // bitcast to integer if it is non integer family
  // because LLVM doesn't support fp type rmw operation
  new_v = build_bitcast_to_integer_type(ty, new_v);
  old_v = build_bitcast_to_integer_type(ty, old_v);
  LLVMValueRef cas_result = LLVMBuildAtomicCmpXchg(
      B, ptr, old_v, new_v, LLVMAtomicOrderingSequentiallyConsistent,
      LLVMAtomicOrderingSequentiallyConsistent, false);
  LLVMValueRef cond =
      LLVMBuildExtractValue(B, cas_result, 1, "armw_cas_result");
  LLVMBuildCondBr(B, cond, bb_merge, bb_body);
  LLVMPositionBuilderAtEnd(B, bb_merge);
  return new_v;
}

static LLVMValueRef llvm_const_one(Type *ty) {
  if (is_flonum(ty)) {
    return LLVMConstReal(type_convert(ty), 1);
  }
  return LLVMConstInt(type_convert(ty), 1, false);
}

static LLVMValueRef gen_expr_post_inc_dec(Node *node, bool is_inc) {
  LLVMValueRef ptr = gen_addr(node->lhs);

  if (node->lhs->ty->kind == TY_PTR) {
    LLVMValueRef result = NULL;
    LLVMValueRef ptr2 = load(node->lhs->ty, ptr);
    if (is_inc) {
      LLVMValueRef one = LLVMConstInt(LLVMInt64TypeInContext(C), 1, false);
      result = LLVMBuildGEP2(B, type_convert(node->lhs->ty->base), ptr2, &one,
                             1, "");
    } else {
      LLVMValueRef neg_one = LLVMConstInt(LLVMInt64TypeInContext(C), -1, true);
      result = LLVMBuildGEP2(B, type_convert(node->lhs->ty->base), ptr2,
                             &neg_one, 1, "");
    }
    store(node->lhs->ty, ptr, result);
    return ptr2;
  }

  LLVMValueRef one = llvm_const_one(node->lhs->ty);
  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);

    LLVMValueRef result = NULL;

    if (is_flonum(node->lhs->ty) && is_inc) {
      result = LLVMBuildFAdd(B, old, one, "");
    } else if (is_flonum(node->lhs->ty) && !is_inc) {
      result = LLVMBuildFSub(B, old, one, "");
    } else if (is_inc) {
      result = LLVMBuildAdd(B, old, one, "");
    } else {
      result = LLVMBuildSub(B, old, one, "");
    }

    bf_store(node->lhs->member, ptr, result);
    return old;
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef old = NULL;
    if (is_flonum(node->lhs->ty)) {
      old = LLVMBuildAtomicRMW(
          B, is_inc ? LLVMAtomicRMWBinOpFAdd : LLVMAtomicRMWBinOpFSub, ptr, one,
          LLVMAtomicOrderingSequentiallyConsistent, false);
    } else {
      old = LLVMBuildAtomicRMW(
          B, is_inc ? LLVMAtomicRMWBinOpAdd : LLVMAtomicRMWBinOpSub, ptr, one,
          LLVMAtomicOrderingSequentiallyConsistent, false);
    }
    return old;
  }

  LLVMValueRef result = NULL;
  LLVMValueRef old = load(node->lhs->ty, ptr);
  if (is_flonum(node->lhs->ty) && is_inc) {
    result = LLVMBuildFAdd(B, old, one, "");
  } else if (is_flonum(node->lhs->ty) && !is_inc) {
    result = LLVMBuildFSub(B, old, one, "");
  } else if (is_inc) {
    result = LLVMBuildAdd(B, old, one, "");
  } else {
    result = LLVMBuildSub(B, old, one, "");
  }
  store(node->lhs->ty, ptr, result);
  return old;
}

static LLVMValueRef gen_expr_cond(Node *node) {
  LLVMValueRef cond = gen_expr(node->cond);
  cond = cmp_nz(cond);
  LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(C, F, "cond_then");
  LLVMBasicBlockRef bb_else = LLVMAppendBasicBlockInContext(C, F, "cond_else");
  LLVMBasicBlockRef bb_merge =
      LLVMAppendBasicBlockInContext(C, F, "cond_merge");
  LLVMBuildCondBr(B, cond, bb_then, bb_else);
  LLVMPositionBuilderAtEnd(B, bb_then);
  LLVMValueRef then_v = gen_expr(node->then);
  if (!then_v) {
    error_tok(node->tok, "there isn't any value returned from true path");
  }
  // update then block which maybe updated by sub-expression
  bb_then = LLVMGetInsertBlock(B);
  llvm_build_terminator_br(bb_merge);
  LLVMPositionBuilderAtEnd(B, bb_else);
  LLVMValueRef else_v = gen_expr(node->_else);
  if (!else_v) {
    error_tok(node->tok, "there isn't any value returned from false path");
  }
  // update else block which maybe updated by sub-expression
  bb_else = LLVMGetInsertBlock(B);
  llvm_build_terminator_br(bb_merge);
  LLVMPositionBuilderAtEnd(B, bb_merge);
  // ternary operator returns void, e.g.
  // 1 ? -2 : (void)-1;
  if (node->ty->kind == TY_VOID) {
    return NULL;
  }
  LLVMTypeRef phi_ty = NULL;
  if (is_agg_type(node->ty)) {
    if (is_large_agg_type(node->ty)) {
      // the large agg value in ternary expression should be treated as ptr.
      // because it's store as ptr type in LLVM
      phi_ty = type_convert(pointer_to(node->ty));
    } else {
      // small agg type, return value directly
      phi_ty = type_convert(node->ty);
    }
  } else {
    phi_ty = type_convert(node->ty);
  }
  LLVMValueRef phi = LLVMBuildPhi(B, phi_ty, "cond_merge_phi");
  LLVMAddIncoming(phi, (LLVMValueRef[]){then_v, else_v},
                  (LLVMBasicBlockRef[]){bb_then, bb_else}, 2);
  return phi;
}

static LLVMValueRef gen_expr_funcall(Node *node) {
  LLVMValueRef F = gen_expr(node->lhs);
  // extract the function type from ponter because LLVM only can recognise
  // this format
  Type *F_ty = node->lhs->ty->base;
  size_t arg_count = next_iter_count(node->args);
  if (is_large_agg_type(F_ty->return_ty)) {
    arg_count++;
  }
  LLVMValueRef *args = calloc(arg_count, sizeof(LLVMValueRef));
  size_t arg_idx = 0;
  if (is_large_agg_type(F_ty->return_ty)) {
    // pass the ret_buffer ptr as the 1st argument
    assert(node->ret_buffer);
    assert(node->ret_buffer->codegen_data);
    args[arg_idx++] = (LLVMValueRef)node->ret_buffer->codegen_data;
  }

  for (Node *arg = node->args; arg; arg = arg->next) {
    args[arg_idx++] = gen_expr(arg);
  }

  bool ret_void = false;
  if (F_ty->return_ty->kind == TY_VOID || is_large_agg_type(F_ty->return_ty)) {
    ret_void = true;
  }

  LLVMValueRef r = LLVMBuildCall2(B, type_convert(F_ty), F, args, arg_count,
                                  ret_void ? "" : "funcall");
  free(args);

  if (is_agg_type(node->ty)) {
    LLVMValueRef ptr = (LLVMValueRef)node->ret_buffer->codegen_data;
    assert(ptr);
    if (!is_large_agg_type(node->ty)) {
      store(node->ty, ptr, r);
      return r;
    }
    // if it's large agg type, when codegen return statement it will memcpy
    // data to ret_buffer ptr
    return ptr;
  }

  return r;
}

static LLVMValueRef gen_expr_ptr_add(Node *node) {
  if (node->lhs->ty->base->kind == TY_VLA) {
    LLVMValueRef ptr = gen_expr(node->lhs);
    LLVMValueRef idx = gen_ptr_index(node->rhs);
    LLVMValueRef step =
        load(node->lhs->ty->base->vla_size->ty,
             (LLVMValueRef)node->lhs->ty->base->vla_size->codegen_data);
    LLVMValueRef bytes = LLVMBuildMul(B, idx, step, "vla_add_bytes");
    return LLVMBuildGEP2(B, LLVMInt8TypeInContext(C), ptr, &bytes, 1,
                         "vla_add");
  }
  Type *pointee_ty =
      node->lhs->ty->base->kind == TY_VOID ? ty_char : node->lhs->ty->base;
  LLVMValueRef base = gen_expr(node->lhs);
  LLVMValueRef idx = gen_ptr_index(node->rhs);
  return LLVMBuildGEP2(B, type_convert(pointee_ty), base, &idx, 1, "ptr_add");
}

static LLVMValueRef gen_expr_ptr_sub(Node *node) {
  if (node->lhs->ty->base->kind == TY_VLA) {
    LLVMValueRef ptr = gen_expr(node->lhs);
    LLVMValueRef idx = gen_ptr_index(node->rhs);
    LLVMValueRef step =
        load(node->lhs->ty->base->vla_size->ty,
             (LLVMValueRef)node->lhs->ty->base->vla_size->codegen_data);
    LLVMValueRef bytes = LLVMBuildMul(B, idx, step, "vla_sub_bytes");
    LLVMValueRef neg = LLVMBuildNeg(B, bytes, "vla_sub_neg");
    return LLVMBuildGEP2(B, LLVMInt8TypeInContext(C), ptr, &neg, 1, "vla_sub");
  }
  LLVMValueRef neg = LLVMBuildNeg(B, gen_ptr_index(node->rhs), "ptr_sub_neg");
  Type *pointee_ty =
      node->lhs->ty->kind == TY_VOID ? ty_char : node->lhs->ty->base;
  return LLVMBuildGEP2(B, type_convert(pointee_ty), gen_expr(node->lhs), &neg,
                       1, "ptr_sub");
}

static LLVMValueRef gen_expr_add(Node *node) {
  if (node->lhs->ty->base) {
    return gen_expr_ptr_add(node);
  } else if (is_flonum(node->lhs->ty)) {
    return LLVMBuildFAdd(B, gen_expr(node->lhs), gen_expr(node->rhs), "fadd");
  } else {
    return LLVMBuildAdd(B, gen_expr(node->lhs), gen_expr(node->rhs), "add");
  }
}

static LLVMValueRef gen_expr_sub(Node *node) {
  if (node->lhs->ty->base) {
    return gen_expr_ptr_sub(node);
  } else if (is_flonum(node->lhs->ty)) {
    return LLVMBuildFSub(B, gen_expr(node->lhs), gen_expr(node->rhs), "fsub");
  } else {
    return LLVMBuildSub(B, gen_expr(node->lhs), gen_expr(node->rhs), "sub");
  }
}

static LLVMValueRef gen_expr_cmp(Node *node) {
  int fop = 0;
  int iop = 0;
  switch (node->kind) {
  case ND_EQ:
    fop = LLVMRealOEQ;
    iop = LLVMIntEQ;
    break;
  case ND_NE:
    // IEEE 754 NaN != NaN
    fop = LLVMRealUNE;
    iop = LLVMIntNE;
    break;
  case ND_LT:
    fop = LLVMRealOLT;
    if (node->lhs->ty->is_unsigned)
      iop = LLVMIntULT;
    else
      iop = LLVMIntSLT;
    break;
  case ND_LE:
    fop = LLVMRealOLE;
    if (node->lhs->ty->is_unsigned)
      iop = LLVMIntULE;
    else
      iop = LLVMIntSLE;
    break;
  }
  LLVMValueRef result = NULL;
  if (is_flonum(node->lhs->ty)) {
    result =
        LLVMBuildFCmp(B, fop, gen_expr(node->lhs), gen_expr(node->rhs), "fcmp");
  } else {
    result =
        LLVMBuildICmp(B, iop, gen_expr(node->lhs), gen_expr(node->rhs), "cmp");
  }

  // We can't use cast here because result has i1 type
  return LLVMBuildZExt(B, result, LLVMInt32TypeInContext(C),
                       "logic_result_zext");
}

static LLVMValueRef gen_expr_sa_ptr_add(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef val = load(node->lhs->ty, ptr);
  LLVMValueRef rhs = gen_ptr_index(node->rhs);
  if (node->lhs->ty->is_atomic) {
    rhs = LLVMBuildMul(B, rhs,
                       LLVMConstInt(LLVMInt64TypeInContext(C),
                                    node->lhs->ty->base->size, false),
                       "");
    LLVMValueRef old_v =
        LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpAdd, ptr, rhs,
                           LLVMAtomicOrderingSequentiallyConsistent, false);
    return LLVMBuildAdd(B, old_v, rhs, "sa_add_ptr_atom_ret_val_add");
  }
  if (node->lhs->ty->base->kind == TY_VLA) {
    LLVMValueRef step =
        load(node->lhs->ty->base->vla_size->ty,
             (LLVMValueRef)node->lhs->ty->base->vla_size->codegen_data);
    LLVMValueRef bytes = LLVMBuildMul(B, rhs, step, "sa_vla_add_bytes");
    LLVMValueRef tmp_v = LLVMBuildGEP2(B, LLVMInt8TypeInContext(C), val, &bytes,
                                       1, "sa_vla_ptr_add");
    store(node->lhs->ty, ptr, tmp_v);
    return load(node->ty, ptr);
  }
  Type *pointee_ty =
      node->lhs->ty->kind == TY_VOID ? ty_char : node->lhs->ty->base;
  LLVMValueRef tmp_v =
      LLVMBuildGEP2(B, type_convert(pointee_ty), val, &rhs, 1, "sa_ptr_add");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_ptr_sub(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef val = load(node->lhs->ty, ptr);
  LLVMValueRef rhs = gen_ptr_index(node->rhs);
  if (node->lhs->ty->is_atomic) {
    rhs = LLVMBuildMul(B, rhs,
                       LLVMConstInt(LLVMInt64TypeInContext(C),
                                    node->lhs->ty->base->size, false),
                       "");
    rhs = LLVMBuildNeg(B, rhs, "");
    LLVMValueRef old_v =
        LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpAdd, ptr, rhs,
                           LLVMAtomicOrderingSequentiallyConsistent, false);
    return LLVMBuildAdd(B, old_v, rhs, "sa_add_ptr_atom_ret_val_add");
  }
  if (node->lhs->ty->base->kind == TY_VLA) {
    LLVMValueRef step =
        load(node->lhs->ty->base->vla_size->ty,
             (LLVMValueRef)node->lhs->ty->base->vla_size->codegen_data);
    LLVMValueRef bytes = LLVMBuildMul(B, rhs, step, "sa_vla_sub_bytes");
    LLVMValueRef neg = LLVMBuildNeg(B, bytes, "sa_vla_sub_neg");
    LLVMValueRef tmp_v = LLVMBuildGEP2(B, LLVMInt8TypeInContext(C), val, &neg,
                                       1, "sa_vla_ptr_sub");
    store(node->lhs->ty, ptr, tmp_v);
    return load(node->ty, ptr);
  }
  LLVMValueRef neg = LLVMBuildNeg(B, rhs, "sa_ptr_sub_neg");
  Type *pointee_ty =
      node->lhs->ty->kind == TY_VOID ? ty_char : node->lhs->ty->base;
  LLVMValueRef tmp_v = LLVMBuildGEP2(B, type_convert(pointee_ty), val,
                                     &(LLVMValueRef){neg}, 1, "sa_ptr_sub");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_add(Node *node) {
  if (node->lhs->ty->base) {
    return gen_expr_sa_ptr_add(node);
  }
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = is_flonum(node->lhs->ty)
                               ? LLVMBuildFAdd(B, old, rhs, "")
                               : LLVMBuildAdd(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef result_v = NULL;
    if (is_flonum(node->lhs->ty)) {
      LLVMValueRef old_v =
          LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpFAdd, ptr, rhs,
                             LLVMAtomicOrderingSequentiallyConsistent, false);
      result_v = LLVMBuildFAdd(B, old_v, rhs, "sa_add_atom_ret_val_fadd");
    } else {
      LLVMValueRef old_v =
          LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpAdd, ptr, rhs,
                             LLVMAtomicOrderingSequentiallyConsistent, false);
      result_v = LLVMBuildAdd(B, old_v, rhs, "sa_add_atom_ret_val_add");
    }
    return result_v;
  }

  LLVMValueRef result_v = NULL;
  if (is_flonum(node->lhs->ty)) {
    result_v = LLVMBuildFAdd(B, load(node->lhs->ty, ptr), rhs, "sa_fadd");
  } else {
    result_v = LLVMBuildAdd(B, load(node->lhs->ty, ptr), rhs, "sa_add");
  }
  store(node->lhs->ty, ptr, result_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_sub(Node *node) {
  if (node->lhs->ty->base) {
    return gen_expr_sa_ptr_sub(node);
  }
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = is_flonum(node->lhs->ty)
                               ? LLVMBuildFSub(B, old, rhs, "")
                               : LLVMBuildSub(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef result_v = NULL;
    if (is_flonum(node->lhs->ty)) {
      LLVMValueRef old_v =
          LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpFSub, ptr, rhs,
                             LLVMAtomicOrderingSequentiallyConsistent, false);
      result_v = LLVMBuildFSub(B, old_v, rhs, "sa_sub_atom_ret_val_fsub");
    } else {
      LLVMValueRef old_v =
          LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpSub, ptr, rhs,
                             LLVMAtomicOrderingSequentiallyConsistent, false);
      result_v = LLVMBuildSub(B, old_v, rhs, "sa_sub_atom_ret_val_sub");
    }
    return result_v;
  }

  LLVMValueRef tmp_v = NULL;
  if (is_flonum(node->lhs->ty)) {
    tmp_v = LLVMBuildFSub(B, load(node->lhs->ty, ptr), rhs, "sa_fsub");
  } else {
    tmp_v = LLVMBuildSub(B, load(node->lhs->ty, ptr), rhs, "sa_sub");
  }
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_mul(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = is_flonum(node->lhs->ty)
                               ? LLVMBuildFMul(B, old, rhs, "")
                               : LLVMBuildMul(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    if (is_flonum(node->lhs->ty)) {
      return build_sa_atomic_rmw(LLVMFMul, node->lhs->ty, ptr, rhs);
    } else {
      return build_sa_atomic_rmw(LLVMMul, node->lhs->ty, ptr, rhs);
    }
  }
  LLVMValueRef tmp_v = NULL;
  if (is_flonum(node->lhs->ty)) {
    tmp_v = LLVMBuildFMul(B, load(node->lhs->ty, ptr), rhs, "sa_fmul");
  } else {
    tmp_v = LLVMBuildMul(B, load(node->lhs->ty, ptr), rhs, "sa_mul");
  }
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_div(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val;
    if (is_flonum(node->lhs->ty))
      new_val = LLVMBuildFDiv(B, old, rhs, "");
    else if (node->lhs->ty->is_unsigned)
      new_val = LLVMBuildUDiv(B, old, rhs, "");
    else
      new_val = LLVMBuildSDiv(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    if (is_flonum(node->lhs->ty)) {
      return build_sa_atomic_rmw(LLVMFDiv, node->lhs->ty, ptr, rhs);
    } else if (node->lhs->ty->is_unsigned) {
      return build_sa_atomic_rmw(LLVMUDiv, node->lhs->ty, ptr, rhs);
    } else {
      return build_sa_atomic_rmw(LLVMSDiv, node->lhs->ty, ptr, rhs);
    }
  }
  LLVMValueRef tmp_v = NULL;
  if (is_flonum(node->lhs->ty)) {
    tmp_v = LLVMBuildFDiv(B, load(node->lhs->ty, ptr), rhs, "sa_fdiv");
  } else if (node->lhs->ty->is_unsigned) {
    tmp_v = LLVMBuildUDiv(B, load(node->lhs->ty, ptr), rhs, "sa_udiv");
  } else {
    tmp_v = LLVMBuildSDiv(B, load(node->lhs->ty, ptr), rhs, "sa_sdiv");
  }
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_mod(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = node->lhs->ty->is_unsigned
                               ? LLVMBuildURem(B, old, rhs, "")
                               : LLVMBuildSRem(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    if (node->lhs->ty->is_unsigned) {
      return build_sa_atomic_rmw(LLVMURem, node->lhs->ty, ptr, rhs);
    } else {
      return build_sa_atomic_rmw(LLVMSRem, node->lhs->ty, ptr, rhs);
    }
  }
  LLVMValueRef tmp_v = NULL;
  if (node->lhs->ty->is_unsigned) {
    tmp_v = LLVMBuildURem(B, load(node->lhs->ty, ptr), rhs, "sa_urem");
  } else {
    tmp_v = LLVMBuildSRem(B, load(node->lhs->ty, ptr), rhs, "sa_srem");
  }
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_bitand(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = LLVMBuildAnd(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef old_v =
        LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpAnd, ptr, rhs,
                           LLVMAtomicOrderingSequentiallyConsistent, false);
    return LLVMBuildAnd(B, old_v, rhs, "sa_bitand_atom_ret_val");
  }

  LLVMValueRef tmp_v =
      LLVMBuildAnd(B, load(node->lhs->ty, ptr), rhs, "sa_bitand");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_bitor(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = LLVMBuildOr(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef old_v =
        LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpOr, ptr, rhs,
                           LLVMAtomicOrderingSequentiallyConsistent, false);
    return LLVMBuildOr(B, old_v, rhs, "sa_bitor_atom_ret_val");
  }

  LLVMValueRef tmp_v =
      LLVMBuildOr(B, load(node->lhs->ty, ptr), rhs, "sa_bitor");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_bitxor(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = LLVMBuildXor(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    LLVMValueRef old_v =
        LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpXor, ptr, rhs,
                           LLVMAtomicOrderingSequentiallyConsistent, false);
    return LLVMBuildXor(B, old_v, rhs, "sa_bitxor_atom_ret_val");
  }

  LLVMValueRef tmp_v =
      LLVMBuildXor(B, load(node->lhs->ty, ptr), rhs, "sa_bitxor");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_shl(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);
  rhs = cast(rhs, node->rhs->ty, node->lhs->ty, node->tok);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = LLVMBuildShl(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    return build_sa_atomic_rmw(LLVMShl, node->lhs->ty, ptr, rhs);
  }
  LLVMValueRef tmp_v = LLVMBuildShl(B, load(node->lhs->ty, ptr), rhs, "sa_shl");
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_sa_shr(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef rhs = gen_expr(node->rhs);
  rhs = cast(rhs, node->lhs->ty, node->rhs->ty, node->tok);

  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    LLVMValueRef old = bf_load(node->lhs->member, ptr);
    LLVMValueRef new_val = node->lhs->ty->is_unsigned
                               ? LLVMBuildLShr(B, old, rhs, "")
                               : LLVMBuildAShr(B, old, rhs, "");
    bf_store(node->lhs->member, ptr, new_val);
    return bf_load(node->lhs->member, ptr);
  }

  if (node->lhs->ty->is_atomic) {
    if (node->lhs->ty->is_unsigned) {
      return build_sa_atomic_rmw(LLVMLShr, node->lhs->ty, ptr, rhs);
    } else {
      return build_sa_atomic_rmw(LLVMAShr, node->lhs->ty, ptr, rhs);
    }
  }
  LLVMValueRef tmp_v = NULL;
  if (node->lhs->ty->is_unsigned) {
    tmp_v = LLVMBuildLShr(B, load(node->lhs->ty, ptr), rhs, "sa_lshr");
  } else {
    tmp_v = LLVMBuildAShr(B, load(node->lhs->ty, ptr), rhs, "sa_ashr");
  }
  store(node->lhs->ty, ptr, tmp_v);
  return load(node->ty, ptr);
}

static LLVMValueRef gen_expr_num(Node *node) {
  switch (node->ty->kind) {
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    return LLVMConstReal(type_convert(node->ty), node->fval);
  default:
    return LLVMConstInt(type_convert(node->ty), node->val,
                        !node->ty->is_unsigned);
  }
}

static LLVMValueRef gen_expr_neg(Node *node) {
  if (is_flonum(node->lhs->ty))
    return LLVMBuildFNeg(B, gen_expr(node->lhs), "fneg");
  return LLVMBuildNeg(B, gen_expr(node->lhs), "neg");
}

static LLVMValueRef gen_expr_var(Node *node) {
  if (node->var->ty->kind == TY_VLA)
    return load(pointer_to(node->var->ty->base), gen_addr(node));
  if (node->var->ty->kind == TY_FUNC || node->var->ty->kind == TY_ARRAY)
    return gen_addr(node);
  return load(node->ty, gen_addr(node));
}

static LLVMValueRef gen_expr_deref(Node *node) {
  if (node->lhs->ty->base && (node->lhs->ty->base->kind == TY_FUNC ||
                              node->lhs->ty->base->kind == TY_ARRAY))
    return gen_expr(node->lhs);
  return load(node->ty, gen_expr(node->lhs));
}

static LLVMValueRef gen_expr_member(Node *node) {
  if (node->member->ty->kind == TY_FUNC || node->member->ty->kind == TY_ARRAY)
    return gen_addr(node);
  if (node->member->is_bitfield) {
    LLVMValueRef ptr = gen_addr(node);
    return bf_load(node->member, ptr);
  }
  return load(node->ty, gen_addr(node));
}

static LLVMValueRef gen_expr_stmt_expr(Node *node) {
  new_block("stmt_expr");
  LLVMValueRef r = NULL;
  for (Node *n = node->body; n; n = n->next)
    r = gen_stmt(n);
  return r;
}

static LLVMValueRef gen_expr_assign(Node *node) {
  LLVMValueRef ptr = gen_addr(node->lhs);
  LLVMValueRef v = gen_expr(node->rhs);
  if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
    bf_store(node->lhs->member, ptr, v);
    return bf_load(node->lhs->member, ptr);
  }
  store(node->ty, ptr, v);
  return load(node->lhs->ty, ptr);
}

static LLVMValueRef gen_expr_not(Node *node) {
  LLVMValueRef v = gen_expr(node->lhs);
  v = cmp_ez(v);
  return LLVMBuildZExt(B, v, LLVMInt32TypeInContext(C), "not_ext");
}

static LLVMValueRef gen_expr_label_val(Node *node) {
  LLVMBasicBlockRef bb = hashmap_get(&block_labels, node->unique_label);
  if (!bb) {
    bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
    hashmap_put(&block_labels, node->unique_label, bb);
  }
  hashmap_put(&func_labels_as_values, node->unique_label, bb);
  return LLVMBlockAddress(F, bb);
}

static LLVMValueRef gen_expr_cas(Node *node) {
  LLVMValueRef ptr = gen_expr(node->cas_addr);
  LLVMValueRef old_ptr = gen_expr(node->cas_old);
  LLVMValueRef new_val = gen_expr(node->cas_new);
  LLVMValueRef old_val = load(node->cas_old->ty->base, old_ptr);
  LLVMValueRef result = LLVMBuildAtomicCmpXchg(
      B, ptr, old_val, new_val, LLVMAtomicOrderingSequentiallyConsistent,
      LLVMAtomicOrderingSequentiallyConsistent, false);
  LLVMValueRef actual_old_val =
      LLVMBuildExtractValue(B, result, 0, "cas_actual_old");
  store(node->cas_old->ty->base, old_ptr, actual_old_val);
  LLVMValueRef success = LLVMBuildExtractValue(B, result, 1, "cas_success");
  return LLVMBuildZExt(B, success, LLVMInt8TypeInContext(C), "cas_success_ext");
}

static LLVMValueRef gen_expr_mul(Node *node) {
  if (is_flonum(node->lhs->ty))
    return LLVMBuildFMul(B, gen_expr(node->lhs), gen_expr(node->rhs), "fmul");
  return LLVMBuildMul(B, gen_expr(node->lhs), gen_expr(node->rhs), "mul");
}

static LLVMValueRef gen_expr_div(Node *node) {
  if (is_flonum(node->lhs->ty))
    return LLVMBuildFDiv(B, gen_expr(node->lhs), gen_expr(node->rhs), "fdiv");
  if (node->lhs->ty->is_unsigned)
    return LLVMBuildUDiv(B, gen_expr(node->lhs), gen_expr(node->rhs), "udiv");
  return LLVMBuildSDiv(B, gen_expr(node->lhs), gen_expr(node->rhs), "sdiv");
}

static LLVMValueRef gen_expr_mod(Node *node) {
  assert(is_integer(node->lhs->ty));
  if (node->lhs->ty->is_unsigned)
    return LLVMBuildURem(B, gen_expr(node->lhs), gen_expr(node->rhs), "umod");
  return LLVMBuildSRem(B, gen_expr(node->lhs), gen_expr(node->rhs), "smod");
}

static LLVMValueRef gen_expr_bitand(Node *node) {
  assert(is_integer(node->lhs->ty));
  return LLVMBuildAnd(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitand");
}

static LLVMValueRef gen_expr_bitor(Node *node) {
  assert(is_integer(node->lhs->ty));
  return LLVMBuildOr(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitor");
}

static LLVMValueRef gen_expr_bitxor(Node *node) {
  assert(is_integer(node->lhs->ty));
  return LLVMBuildXor(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitxor");
}

static LLVMValueRef gen_expr_shl(Node *node) {
  LLVMValueRef rhs = gen_expr(node->rhs);
  rhs = cast(rhs, node->rhs->ty, node->lhs->ty, node->tok);
  return LLVMBuildShl(B, gen_expr(node->lhs), rhs, "shl");
}

static LLVMValueRef gen_expr_shr(Node *node) {
  LLVMValueRef rhs = gen_expr(node->rhs);
  rhs = cast(rhs, node->rhs->ty, node->lhs->ty, node->tok);
  if (node->lhs->ty->is_unsigned)
    return LLVMBuildLShr(B, gen_expr(node->lhs), rhs, "lshr");
  return LLVMBuildAShr(B, gen_expr(node->lhs), rhs, "ashr");
}

static LLVMValueRef gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NULL_EXPR: {
    return NULL;
  }
  case ND_NUM: {
    return gen_expr_num(node);
  }
  case ND_NEG: {
    return gen_expr_neg(node);
  }
  case ND_CAST: {
    return cast(gen_expr(node->lhs), node->lhs->ty, node->ty, node->tok);
  }
  case ND_VAR: {
    return gen_expr_var(node);
  }
  case ND_DEREF: {
    return gen_expr_deref(node);
  }
  case ND_ADDR: {
    return gen_addr(node->lhs);
  }
  case ND_MEMBER: {
    return gen_expr_member(node);
  }
  case ND_STMT_EXPR: {
    return gen_expr_stmt_expr(node);
  }
  case ND_COMMA:
    gen_expr(node->lhs);
    return gen_expr(node->rhs);
  case ND_ASSIGN: {
    return gen_expr_assign(node);
  }
  case ND_MEMZERO: {
    assert(node->var->codegen_data);
    llvm_memset((LLVMValueRef)node->var->codegen_data, 0, node->var->ty->size,
                false);
    return NULL;
  }
  case ND_COND: {
    return gen_expr_cond(node);
  }
  case ND_BITNOT: {
    LLVMValueRef v = gen_expr(node->lhs);
    return LLVMBuildNot(B, v, "bitnot");
  }
  case ND_NOT: {
    return gen_expr_not(node);
  }
  case ND_LOGAND: {
    return logic_short_circuit(node, true);
  }
  case ND_LOGOR: {
    return logic_short_circuit(node, false);
  }
  case ND_ALLOCA: {
    LLVMValueRef sz = gen_expr(node->lhs);
    return LLVMBuildArrayAlloca(B, LLVMInt8TypeInContext(C), sz,
                                "builtin_alloca");
  }
  case ND_FUNCALL: {
    assert(node->lhs->ty->kind == TY_PTR);
    return gen_expr_funcall(node);
  }
  case ND_LABEL_VAL: {
    return gen_expr_label_val(node);
  }
  case ND_EXCH: {
    LLVMValueRef ptr = gen_expr(node->lhs);
    LLVMValueRef new_val = gen_expr(node->rhs);
    return LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpXchg, ptr, new_val,
                              LLVMAtomicOrderingSequentiallyConsistent, false);
  }
  case ND_CAS: {
    return gen_expr_cas(node);
  }
  case ND_ADD: {
    return gen_expr_add(node);
  }
  case ND_SUB: {
    return gen_expr_sub(node);
  }
  case ND_MUL: {
    return gen_expr_mul(node);
  }
  case ND_DIV: {
    return gen_expr_div(node);
  }
  case ND_MOD: {
    return gen_expr_mod(node);
  }
  case ND_BITAND: {
    return gen_expr_bitand(node);
  }
  case ND_BITOR: {
    return gen_expr_bitor(node);
  }
  case ND_BITXOR: {
    return gen_expr_bitxor(node);
  }
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    return gen_expr_cmp(node);
  }
  case ND_SHL: {
    return gen_expr_shl(node);
  }
  case ND_SHR: {
    return gen_expr_shr(node);
  }
  case ND_SA_ADD: {
    return gen_expr_sa_add(node);
  }
  case ND_POST_INC: {
    return gen_expr_post_inc_dec(node, true);
  }
  case ND_POST_DEC: {
    return gen_expr_post_inc_dec(node, false);
  }
  case ND_SA_SUB: {
    return gen_expr_sa_sub(node);
  }
  case ND_SA_MUL: {
    return gen_expr_sa_mul(node);
  }
  case ND_SA_DIV: {
    return gen_expr_sa_div(node);
  }
  case ND_SA_MOD: {
    return gen_expr_sa_mod(node);
  }
  case ND_SA_BITAND: {
    return gen_expr_sa_bitand(node);
  }
  case ND_SA_BITOR: {
    return gen_expr_sa_bitor(node);
  }
  case ND_SA_BITXOR: {
    return gen_expr_sa_bitxor(node);
  }
  case ND_SA_SHL: {
    return gen_expr_sa_shl(node);
  }
  case ND_SA_SHR: {
    return gen_expr_sa_shr(node);
  }
  case ND_VA_START: {
    llvm_va_start(gen_expr(node->lhs));
    return NULL;
  }
  case ND_VA_END: {
    llvm_va_end(gen_expr(node->lhs));
    return NULL;
  }
  case ND_VA_COPY: {
    llvm_va_copy(gen_expr(node->lhs), gen_expr(node->rhs));
    return NULL;
  }
  case ND__SEQ: {
    LLVMValueRef r = NULL;
    for (size_t idx = 0; idx < node->_seq->len; idx++) {
      Node *n = node->_seq->data[idx];
      r = gen_expr(n);
    }
    return r;
  }
  }
  error_tok(node->tok, "invalid expression");
}

static LLVMValueRef gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_BLOCK: {
    new_block("block");
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return NULL;
  }
  case ND_RETURN: {
    new_block("return");
    if (!node->lhs) {
      // return;
      LLVMBuildRetVoid(B);
      return NULL;
    }
    if (is_agg_type(node->lhs->ty)) {
      LLVMValueRef v = gen_expr(node->lhs);
      if (is_large_agg_type(node->lhs->ty)) {
        // it's not very standard
        // now v is agg ptr
        llvm_memcpy(LLVMGetParam(F, 0), v, node->lhs->ty->size, false);
        LLVMBuildRetVoid(B);
        return NULL;
      }
      // small agg, v is value
      LLVMBuildRet(B, v);
      return NULL;
    }
    LLVMValueRef ret = gen_expr(node->lhs);
    if (node->lhs->ty->kind == TY_VOID) {
      LLVMBuildRetVoid(B);
    } else {
      LLVMBuildRet(B, ret);
    }
    return NULL;
  }
  case ND_EXPR_STMT: {
    new_block("expr_stmt");
    return gen_expr(node->lhs);
  }
  case ND_IF: {
    new_block("if");
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_nz(cond);
    LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(C, F, "if_then");
    LLVMBasicBlockRef bb_else = LLVMAppendBasicBlockInContext(C, F, "if_else");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "if_merge");
    LLVMBuildCondBr(B, cond, bb_then, bb_else);
    LLVMPositionBuilderAtEnd(B, bb_then);
    gen_stmt(node->then);
    llvm_build_terminator_br(bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_else);
    if (node->_else) {
      gen_stmt(node->_else);
    }
    llvm_build_terminator_br(bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_FOR: {
    new_block("for");

    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "for_merge");
    LLVMBasicBlockRef bb_cond = LLVMAppendBasicBlockInContext(C, F, "for_cond");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(C, F, "for_body");
    LLVMBasicBlockRef bb_inc = LLVMAppendBasicBlockInContext(C, F, "for_inc");
    hashmap_put(&block_labels, node->break_label, bb_merge);
    hashmap_put(&block_labels, node->cont_label, bb_inc);
    if (node->init) {
      gen_stmt(node->init);
    }
    llvm_build_terminator_br(bb_cond);
    LLVMPositionBuilderAtEnd(B, bb_cond);
    if (node->cond) {
      LLVMValueRef cond = gen_expr(node->cond);
      cond = cmp_nz(cond);
      LLVMBuildCondBr(B, cond, bb_body, bb_merge);
    } else {
      llvm_build_terminator_br(bb_body);
    }
    LLVMPositionBuilderAtEnd(B, bb_body);
    gen_stmt(node->then);
    llvm_build_terminator_br(bb_inc);
    LLVMPositionBuilderAtEnd(B, bb_inc);

    if (node->inc)
      gen_expr(node->inc);
    llvm_build_terminator_br(bb_cond);

    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_DO: {
    new_block("do");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(C, F, "do_body");
    LLVMBasicBlockRef bb_cond = LLVMAppendBasicBlockInContext(C, F, "do_cond");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "do_merge");
    hashmap_put(&block_labels, node->break_label, bb_merge);
    hashmap_put(&block_labels, node->cont_label, bb_cond);
    llvm_build_terminator_br(bb_body);
    LLVMPositionBuilderAtEnd(B, bb_body);
    gen_stmt(node->then);
    llvm_build_terminator_br(bb_cond);
    LLVMPositionBuilderAtEnd(B, bb_cond);
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_nz(cond);
    LLVMBuildCondBr(B, cond, bb_body, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_LABEL: {
    new_block("labeled_stmt");
    LLVMBasicBlockRef bb = hashmap_get(&block_labels, node->unique_label);
    if (!bb) {
      // if bb exists, goto statement create this before.
      // we just reuse this, or we  create a new one
      bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
      hashmap_put(&block_labels, node->unique_label, bb);
    }
    llvm_build_terminator_br(bb);
    LLVMPositionBuilderAtEnd(B, bb);
    return gen_stmt(node->lhs);
  }
  case ND_GOTO: {
    new_block("goto");

    LLVMBasicBlockRef bb = hashmap_get(&block_labels, node->unique_label);
    if (!bb) {
      // if bb exists, labeled statement create this before.
      // we just reuse this, or we  create a new one
      bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
      hashmap_put(&block_labels, node->unique_label, bb);
    }
    llvm_build_terminator_br(bb);
    // we keep current Builder position because the later code is
    // not necessary just like those ir after br in same block.
    return NULL;
  }
  case ND_SWITCH: {
    new_block("switch");
    LLVMBasicBlockRef bb_after =
        LLVMAppendBasicBlockInContext(C, F, "switch_after");
    assert(node->break_label);
    hashmap_put(&block_labels, node->break_label, bb_after);
    LLVMBasicBlockRef bb_body =
        LLVMAppendBasicBlockInContext(C, F, "switch_body");
    LLVMBasicBlockRef bb_cond =
        LLVMAppendBasicBlockInContext(C, F, "switch_cond");
    llvm_build_terminator_br(bb_cond);
    LLVMPositionBuilderAtEnd(B, bb_body);
    gen_stmt(node->then);
    llvm_build_terminator_br(bb_after);
    LLVMPositionBuilderAtEnd(B, bb_cond);
    gen_switch_cmp_algo_rev_direct(node, bb_after);
    LLVMPositionBuilderAtEnd(B, bb_after);
    return NULL;
  }
  case ND_CASE: {
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(C, F, "case");
    hashmap_put(&block_labels, node->label, bb);
    llvm_build_terminator_br(bb);
    LLVMPositionBuilderAtEnd(B, bb);
    gen_stmt(node->lhs);

    return NULL;
  }
  case ND_GOTO_EXPR: {
    LLVMValueRef addr = gen_expr(node->lhs);
    LLVMValueRef indirect_br =
        LLVMBuildIndirectBr(B, addr, func_labels_as_values.used);
    hashmap_foreach(&func_labels_as_values, L) {
      assert(L->val);
      LLVMAddDestination(indirect_br, L->val);
    }
    return NULL;
  }
  case ND_ASM: {
    // TODO: full gnu asm support
    // escape $ to $$
    int len = strlen(node->asm_str);
    int new_len = 0;
    for (int i = 0; i < len; i++) {
      if (node->asm_str[i] == '$')
        new_len++;
      new_len++;
    }
    char *escaped = calloc(new_len + 1, sizeof(char));
    int j = 0;
    for (int i = 0; i < len; i++) {
      if (node->asm_str[i] == '$')
        escaped[j++] = '$';
      escaped[j++] = node->asm_str[i];
    }

    LLVMTypeRef void_ty = LLVMVoidTypeInContext(C);
    LLVMTypeRef func_type = LLVMFunctionType(void_ty, NULL, 0, 0);
    LLVMValueRef inline_asm =
        LLVMGetInlineAsm(func_type, escaped, new_len, "", 0, true, false,
                         LLVMInlineAsmDialectATT, false);
    free(escaped);
    LLVMBuildCall2(B, func_type, inline_asm, NULL, 0, "");
    return NULL;
  }
  default:
    unreachable();
  }
}

// directly reversed traverse all case
static void gen_switch_cmp_algo_rev_direct(Node *node,
                                           LLVMBasicBlockRef switch_after) {
  LLVMValueRef v = gen_expr(node->cond);

  for (Node *n = node->case_next; n; n = n->case_next) {
    LLVMBasicBlockRef bb_case = hashmap_get(&block_labels, n->label);
    assert(bb_case);
    LLVMValueRef case_cmp = NULL;
    if (n->begin == n->end) {
      case_cmp =
          LLVMBuildICmp(B, LLVMIntEQ, v,
                        LLVMConstInt(LLVMInt64TypeInContext(C), n->begin, true),
                        "switch_cmp");
    } else {
      // gnu ext: case range
      LLVMValueRef sub = LLVMBuildSub(
          B, v, LLVMConstInt(LLVMInt64TypeInContext(C), n->begin, true),
          "switch_range_sub");
      case_cmp = LLVMBuildICmp(
          B, LLVMIntULE, sub,
          LLVMConstInt(LLVMInt64TypeInContext(C), n->end - n->begin, true),
          "switch_rang_cmp");
    }
    LLVMBasicBlockRef case_after =
        LLVMAppendBasicBlockInContext(C, F, "case_after");
    LLVMBuildCondBr(B, case_cmp, bb_case, case_after);
    LLVMPositionBuilderAtEnd(B, case_after);
  }
  if (node->default_case) {
    LLVMBasicBlockRef bb_default =
        hashmap_get(&block_labels, node->default_case->label);
    llvm_build_terminator_br(bb_default);
  } else {
    llvm_build_terminator_br(switch_after);
  }
}

// check whether this function declared agg argument or return agg type
static bool is_function_agg_declare(Obj *var) {
  if (!var->is_function) {
    return false;
  }

  if (is_agg_type(var->ty->return_ty)) {
    return true;
  }

  for (Type *p = var->ty->params; p; p = p->next) {
    if (is_agg_type(p))
      return true;
  }
  return false;
}

// this function will not modify var->codegen_data,instead this function just
// return the LLVMValueRef and codegen_global_declare will do it.
static LLVMValueRef declare_agg_function(Obj *var) {
  assert(var->is_function);
  Type *ty = var->ty;

  LLVMValueRef func =
      LLVMAddFunction(M, get_var_real_name(var), type_convert(ty));

  // attach 'byval' label
  LLVMAttributeIndex params_idx = 1;

  if (is_large_agg_type(ty->return_ty)) {
    // large agg type will occur the first param to pass the ptr
    params_idx++;
  }

  for (Type *p = ty->params; p; p = p->next) {
    if (is_large_agg_type(p)) {
      unsigned kind_id = LLVMGetEnumAttributeKindForName("byval", 5);
      LLVMTypeRef actual_struct_type = type_convert(p);
      LLVMAttributeRef byval_attr =
          LLVMCreateTypeAttribute(C, kind_id, actual_struct_type);

      LLVMAddAttributeAtIndex(func, params_idx, byval_attr);
    }
    params_idx++;
  }
  return func;
}

static void codegen_alloca_function_local_argument(Obj *args) {
  size_t args_count = 0;
  for (Obj *p = args; p; p = p->next) {
    LLVMValueRef arg_vr = NULL;
    LLVMValueRef arg = LLVMGetParam(F, args_count);
    Type *alloc_ty = p->ty->kind == TY_VLA ? pointer_to(p->ty->base) : p->ty;
    if (is_agg_type(p->ty)) {
      if (is_large_agg_type(p->ty)) {
        // gep arg only
        arg_vr = LLVMBuildInBoundsGEP2(B, type_convert(p->ty), arg, NULL, 0,
                                       "lagg_arg_gep");
      } else {
        // same as normal variable
        // TODO: correct system v implementation
        arg_vr = LLVMBuildAlloca(B, type_convert(alloc_ty), p->name);
        // store arg to alloca variable
        store(alloc_ty, arg_vr, arg);
      }
    } else {
      arg_vr = LLVMBuildAlloca(B, type_convert(alloc_ty), p->name);
      // store arg to alloca variable
      store(alloc_ty, arg_vr, arg);
    }
    p->codegen_data = (intptr_t)arg_vr;
    args_count++;
  }
}

// Recurisvely declare them in reversed order, so that the codegen result will
// keep a same order as source code
static void alloca_function_local_variable(Obj *local_vars) {

  if ((!local_vars) || local_vars->codegen_data)
    return;
  alloca_function_local_variable(local_vars->next);
  Type *alloc_ty = local_vars->ty->kind == TY_VLA
                       ? pointer_to(local_vars->ty->base)
                       : local_vars->ty;
  LLVMValueRef lv =
      LLVMBuildAlloca(B, type_convert(alloc_ty), local_vars->name);
  local_vars->codegen_data = (intptr_t)lv;
}

static void build_function_default_return(Obj *var) {
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B))) {
    // If there isn't any terminator(return) in the last BB, create a new one
    if (var->ty->return_ty->kind == TY_VOID) {
      LLVMBuildRetVoid(B);
    } else if (is_large_agg_type(var->ty->return_ty)) {
      LLVMBuildRetVoid(B);
    } else {
      LLVMBuildRet(B, LLVMConstNull(type_convert(var->ty->return_ty)));
    }
  }
}

static void restore_function_labels(LLVMValueRef fn) {
  // Restore labels for the current function from block_labels
  hashmap_clear(&func_labels_as_values);
  hashmap_foreach(&block_labels, entry) {
    LLVMBasicBlockRef bb = (LLVMBasicBlockRef)entry->val;
    if (LLVMGetBasicBlockParent(bb) == fn) {
      hashmap_put(&func_labels_as_values, entry->key, bb);
    }
  }
}

static void build_function_body(Obj *fn) {
  F = (LLVMValueRef)fn->codegen_data;
  assert(F);
  // Restore labels for the current function that were created during global
  // initialization
  // e.g. static void *p[]={&&l1,&&l2,&&l3};
  restore_function_labels(F);
  // prologue
  LLVMBasicBlockRef entry = LLVMGetFirstBasicBlock(F);
  assert(entry);
  LLVMPositionBuilderAtEnd(B, entry);
  codegen_alloca_function_local_argument(fn->params);
  alloca_function_local_variable(fn->locals);
  gen_stmt(fn->body);
  build_function_default_return(fn);
  F = NULL;
  hashmap_clear(&func_labels_as_values);
}

static void ensure_function_entry_block(LLVMValueRef fn) {
  LLVMAppendBasicBlockInContext(C, fn, "entry");
}

// stage 1. Only declare global variable to avoid dependency order
// problem.
// Recurisvely declare them in reversed order, so that the codegen result will
// keep a same order as source code
static void codegen_global_declare(Obj *var) {
  if (!var)
    return;
  codegen_global_declare(var->next);

  if (!var->is_live)
    return;

  LLVMValueRef vr = NULL;
  if (var->is_function) {
    if (is_function_agg_declare(var)) {
      // So here we should not use the type_convert to build llvm function
      // declaration because we can't use it to solve agg param/return type
      vr = declare_agg_function(var);
    } else {
      LLVMTypeRef ty = type_convert(var->ty);
      vr = LLVMAddFunction(M, get_var_real_name(var), ty);
    }
    if (var->is_definition) {
      // create entry block early to avoid some blocks that created in global
      // declare process be the first block. e.g. int F(){ static void
      // *p[]={&&v41,&&v42,&&v43}; int i=0; goto *p[0]; v41:i++; v42:i++;
      // v43:i++; i;
      // }
      // it will create v41, v42, v43 block in global declare process and if
      // there isn't entry block those block will be the first block
      ensure_function_entry_block(vr);
    }
  } else {
    LLVMTypeRef ty = type_convert(var->ty);
    vr = LLVMAddGlobal(M, ty, get_var_real_name(var));
  }
  llvm_set_value_attr(var, vr);
  var->codegen_data = (intptr_t)vr;
}

// stage 2. initialize global variable
static void codegen_global_init(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (!var->is_live) {
      continue;
    }
    if (!var->is_function) {
      LLVMValueRef old_v = (LLVMValueRef)var->codegen_data;
      assert(old_v);
      if (var->init) {
        LLVMValueRef init_val = init_global_data(var->ty, var->init);
        LLVMTypeRef init_ty = LLVMTypeOf(init_val);
        LLVMTypeRef declared_ty = type_convert(var->ty);
        // used to resolve fucking union init problem
        if (init_ty != declared_ty) {
          // create a new variable with new specific type
          LLVMValueRef new_v = LLVMAddGlobal(M, init_ty, "");
          LLVMSetInitializer(new_v, init_val);

          // copy attribute
          llvm_set_value_attr(var, new_v);

          // update reference in llvm system
          LLVMReplaceAllUsesWith(old_v, new_v);

          LLVMDeleteGlobal(old_v);
          // Re-set the name after deleting old_v so the symbol
          // has the correct name (not e.g. "v.1").
          LLVMSetValueName(new_v, get_var_real_name(var));
          // update reference in our system
          var->codegen_data = (intptr_t)new_v;
        } else {
          LLVMSetInitializer(old_v, init_val);
        }
      } else {
        if (var->is_definition)
          LLVMSetInitializer(old_v, LLVMConstNull(type_convert(var->ty)));
      }
      continue;
    }
    if (var->is_function && var->body) {
      build_function_body(var);
      continue;
    }
  }
}

void codegen(Obj *prog, FILE *out, bool gen_asm) {
  C = LLVMContextCreate();
  char *module_id = opt_cc1_filename ?: "<unknown>";
  M = LLVMModuleCreateWithNameInContext(module_id, C);
  B = LLVMCreateBuilderInContext(C);

  codegen_global_declare(prog);

  codegen_global_init(prog);
  if (!opt_skip_verify) {
    if (LLVMVerifyModule(M, LLVMPrintMessageAction, NULL)) {
      error("Failed to verify module %s", module_id);
    }
  }
  if (gen_asm) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    char *triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target = NULL;
    char *err = NULL;
    if (LLVMGetTargetFromTriple(triple, &target, &err)) {
      error("LLVM backend failed to get target machine from triple: %s", err);
    }

    LLVMTargetMachineRef T = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault,
        opt_fpic ? LLVMRelocPIC : LLVMRelocStatic, LLVMCodeModelSmall);
    LLVMMemoryBufferRef asm_output_buffer = NULL;
    if (LLVMTargetMachineEmitToMemoryBuffer(T, M, LLVMAssemblyFile, &err,
                                            &asm_output_buffer)) {

      error("LLVM backend failed to emit memory buffer: %s", err);
    }
    const char *asm_data = LLVMGetBufferStart(asm_output_buffer);
    size_t asm_size = LLVMGetBufferSize(asm_output_buffer);
    fwrite(asm_data, sizeof(char), asm_size, out);
    fflush(out);
    LLVMDisposeMemoryBuffer(asm_output_buffer);
    LLVMDisposeTargetMachine(T);
    LLVMDisposeMessage(triple);
  } else {
    // print IR to out
    char *ir = LLVMPrintModuleToString(M);
    fputs(ir, out);
    fflush(out);
    LLVMDisposeMessage(ir);
  }

  // cleanup
  LLVMDisposeBuilder(B);
  LLVMDisposeModule(M);
  LLVMContextDispose(C);
}
