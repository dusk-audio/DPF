/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2021 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DAF_DEFINES_H_INCLUDED
#define DAF_DEFINES_H_INCLUDED

/* Compatibility with non-clang compilers */
#ifndef __has_feature
# define __has_feature(x) 0
#endif
#ifndef __has_extension
# define __has_extension __has_feature
#endif

/* Check OS */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
# define DAF_API
# define DAF_PLUGIN_EXPORT extern "C" __declspec (dllexport)
# define DAF_OS_WINDOWS 1
# define DAF_DLL_EXTENSION "dll"
#else
# define DAF_API
# define DAF_PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))
# if defined(__APPLE__)
#  define DAF_OS_MAC 1
#  define DAF_DLL_EXTENSION "dylib"
# elif defined(__HAIKU__)
#  define DAF_OS_HAIKU 1
# elif defined(__linux__) || defined(__linux)
#  define DAF_OS_LINUX 1
# elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  define DAF_OS_BSD 1
# elif defined(__GNU__)
#  define DAF_OS_GNU_HURD 1
# elif defined(__EMSCRIPTEN__)
#  define DAF_OS_WASM 1
# endif
#endif

#ifndef DAF_DLL_EXTENSION
# define DAF_DLL_EXTENSION "so"
#endif

/* Check for C++11 support */
#if defined(HAVE_CPP11_SUPPORT)
# if HAVE_CPP11_SUPPORT
#  define DAF_PROPER_CPP11_SUPPORT
# endif
#elif __cplusplus >= 201103L || (defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 405) || __has_extension(cxx_noexcept) || (defined(_MSC_VER) && _MSVC_LANG >= 201103L)
# define DAF_PROPER_CPP11_SUPPORT
# if (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 407 && ! defined(__clang__)) || (defined(__clang__) && ! __has_extension(cxx_override_control))
#  define override /* gcc4.7+ only */
#  define final    /* gcc4.7+ only */
# endif
#endif

#ifndef DAF_PROPER_CPP11_SUPPORT
# define constexpr
# define noexcept throw()
# define override
# define final
# define nullptr NULL
#endif

/* Define unlikely */
#ifdef __GNUC__
# define unlikely(x) __builtin_expect(x,0)
#else
# define unlikely(x) x
#endif

/* Define DAF_DEPRECATED */
#if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 480
# define DAF_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
# define DAF_DEPRECATED [[deprecated]] /* Note: __declspec(deprecated) it not applicable to enum members */
#else
# define DAF_DEPRECATED
#endif

/* Define DAF_DEPRECATED_BY */
#if defined(__clang__) && (__clang_major__ * 100 + __clang_minor__) >= 502
# define DAF_DEPRECATED_BY(other) __attribute__((deprecated("", other)))
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 408
# define DAF_DEPRECATED_BY(other) __attribute__((deprecated("Use " other)))
#else
# define DAF_DEPRECATED_BY(other) DAF_DEPRECATED
#endif

/* Define DAF_SAFE_ASSERT* */
#define DAF_SAFE_ASSERT(cond)               if (unlikely(!(cond))) d_safe_assert      (#cond, __FILE__, __LINE__);
#define DAF_SAFE_ASSERT_INT(cond, value)    if (unlikely(!(cond))) d_safe_assert_int  (#cond, __FILE__, __LINE__, static_cast<int>(value));
#define DAF_SAFE_ASSERT_INT2(cond, v1, v2)  if (unlikely(!(cond))) d_safe_assert_int2 (#cond, __FILE__, __LINE__, static_cast<int>(v1), static_cast<int>(v2));
#define DAF_SAFE_ASSERT_UINT(cond, value)   if (unlikely(!(cond))) d_safe_assert_uint (#cond, __FILE__, __LINE__, static_cast<uint>(value));
#define DAF_SAFE_ASSERT_UINT2(cond, v1, v2) if (unlikely(!(cond))) d_safe_assert_uint2(#cond, __FILE__, __LINE__, static_cast<uint>(v1), static_cast<uint>(v2));

#define DAF_SAFE_ASSERT_BREAK(cond)         if (unlikely(!(cond))) { d_safe_assert(#cond, __FILE__, __LINE__); break; }
#define DAF_SAFE_ASSERT_CONTINUE(cond)      if (unlikely(!(cond))) { d_safe_assert(#cond, __FILE__, __LINE__); continue; }
#define DAF_SAFE_ASSERT_RETURN(cond, ret)   if (unlikely(!(cond))) { d_safe_assert(#cond, __FILE__, __LINE__); return ret; }

#define DAF_CUSTOM_SAFE_ASSERT(msg, cond)             if (unlikely(!(cond)))   d_custom_safe_assert(msg, #cond, __FILE__, __LINE__);
#define DAF_CUSTOM_SAFE_ASSERT_BREAK(msg, cond)       if (unlikely(!(cond))) { d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); break; }
#define DAF_CUSTOM_SAFE_ASSERT_CONTINUE(msg, cond)    if (unlikely(!(cond))) { d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); continue; }
#define DAF_CUSTOM_SAFE_ASSERT_RETURN(msg, cond, ret) if (unlikely(!(cond))) { d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); return ret; }

#define DAF_CUSTOM_SAFE_ASSERT_ONCE_BREAK(msg, cond)       if (unlikely(!(cond))) { static bool _p; if (!_p) { _p = true; d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); } break; }
#define DAF_CUSTOM_SAFE_ASSERT_ONCE_CONTINUE(msg, cond)    if (unlikely(!(cond))) { static bool _p; if (!_p) { _p = true; d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); } continue; }
#define DAF_CUSTOM_SAFE_ASSERT_ONCE_RETURN(msg, cond, ret) if (unlikely(!(cond))) { static bool _p; if (!_p) { _p = true; d_custom_safe_assert(msg, #cond, __FILE__, __LINE__); } return ret; }

#define DAF_SAFE_ASSERT_INT_BREAK(cond, value)       if (unlikely(!(cond))) { d_safe_assert_int(#cond, __FILE__, __LINE__, static_cast<int>(value)); break; }
#define DAF_SAFE_ASSERT_INT_CONTINUE(cond, value)    if (unlikely(!(cond))) { d_safe_assert_int(#cond, __FILE__, __LINE__, static_cast<int>(value)); continue; }
#define DAF_SAFE_ASSERT_INT_RETURN(cond, value, ret) if (unlikely(!(cond))) { d_safe_assert_int(#cond, __FILE__, __LINE__, static_cast<int>(value)); return ret; }

#define DAF_SAFE_ASSERT_INT2_BREAK(cond, v1, v2)        if (unlikely(!(cond))) { d_safe_assert_int2(#cond, __FILE__, __LINE__, static_cast<int>(v1), static_cast<int>(v2)); break; }
#define DAF_SAFE_ASSERT_INT2_CONTINUE(cond, v1, v2)     if (unlikely(!(cond))) { d_safe_assert_int2(#cond, __FILE__, __LINE__, static_cast<int>(v1), static_cast<int>(v2)); continue; }
#define DAF_SAFE_ASSERT_INT2_RETURN(cond, v1, v2, ret)  if (unlikely(!(cond))) { d_safe_assert_int2(#cond, __FILE__, __LINE__, static_cast<int>(v1), static_cast<int>(v2)); return ret; }

#define DAF_SAFE_ASSERT_UINT_BREAK(cond, value)       if (unlikely(!(cond))) { d_safe_assert_uint(#cond, __FILE__, __LINE__, static_cast<uint>(value)); break; }
#define DAF_SAFE_ASSERT_UINT_CONTINUE(cond, value)    if (unlikely(!(cond))) { d_safe_assert_uint(#cond, __FILE__, __LINE__, static_cast<uint>(value)); continue; }
#define DAF_SAFE_ASSERT_UINT_RETURN(cond, value, ret) if (unlikely(!(cond))) { d_safe_assert_uint(#cond, __FILE__, __LINE__, static_cast<uint>(value)); return ret; }

#define DAF_SAFE_ASSERT_UINT2_BREAK(cond, v1, v2)       if (unlikely(!(cond))) { d_safe_assert_uint2(#cond, __FILE__, __LINE__, static_cast<uint>(v1), static_cast<uint>(v2)); break; }
#define DAF_SAFE_ASSERT_UINT2_CONTINUE(cond, v1, v2)    if (unlikely(!(cond))) { d_safe_assert_uint2(#cond, __FILE__, __LINE__, static_cast<uint>(v1), static_cast<uint>(v2)); continue; }
#define DAF_SAFE_ASSERT_UINT2_RETURN(cond, v1, v2, ret) if (unlikely(!(cond))) { d_safe_assert_uint2(#cond, __FILE__, __LINE__, static_cast<uint>(v1), static_cast<uint>(v2)); return ret; }

/* Define DAF_SAFE_EXCEPTION */
#define DAF_SAFE_EXCEPTION(msg)             catch(...) { d_safe_exception(msg, __FILE__, __LINE__); }
#define DAF_SAFE_EXCEPTION_BREAK(msg)       catch(...) { d_safe_exception(msg, __FILE__, __LINE__); break; }
#define DAF_SAFE_EXCEPTION_CONTINUE(msg)    catch(...) { d_safe_exception(msg, __FILE__, __LINE__); continue; }
#define DAF_SAFE_EXCEPTION_RETURN(msg, ret) catch(...) { d_safe_exception(msg, __FILE__, __LINE__); return ret; }

/* Define DAF_DECLARE_NON_COPYABLE */
#ifdef DAF_PROPER_CPP11_SUPPORT
# define DAF_DECLARE_NON_COPYABLE(ClassName) \
private:                                         \
    ClassName(ClassName&) = delete;              \
    ClassName(const ClassName&) = delete;        \
    ClassName& operator=(ClassName&) = delete;   \
    ClassName& operator=(const ClassName&) = delete;
#else
# define DAF_DECLARE_NON_COPYABLE(ClassName) \
private:                                         \
    ClassName(ClassName&);                       \
    ClassName(const ClassName&);                 \
    ClassName& operator=(ClassName&);            \
    ClassName& operator=(const ClassName&);
#endif

/* Define DAF_PREVENT_HEAP_ALLOCATION */
#ifdef DAF_PROPER_CPP11_SUPPORT
# define DAF_PREVENT_HEAP_ALLOCATION        \
private:                                        \
    static void* operator new(size_t) = delete; \
    static void operator delete(void*) = delete;
#else
# define DAF_PREVENT_HEAP_ALLOCATION \
private:                                 \
    static void* operator new(size_t);   \
    static void operator delete(void*);
#endif

/* Define DAF_PREVENT_VIRTUAL_HEAP_ALLOCATION */
#ifdef DAF_PROPER_CPP11_SUPPORT
# define DAF_PREVENT_VIRTUAL_HEAP_ALLOCATION \
private:                                         \
    static void* operator new(std::size_t) = delete;
#else
# define DAF_PREVENT_VIRTUAL_HEAP_ALLOCATION \
private:                                         \
    static void* operator new(std::size_t);
#endif

/* Define namespace */
#ifndef DAF_NAMESPACE
# define DAF_NAMESPACE DAF
#endif
#define START_NAMESPACE_DAF namespace DAF_NAMESPACE {
#define END_NAMESPACE_DAF }
#define USE_NAMESPACE_DAF using namespace DAF_NAMESPACE;

/* Define DAF_OS_SEP and DAF_OS_SPLIT */
#ifdef DAF_OS_WINDOWS
# define DAF_OS_SEP       '\\'
# define DAF_OS_SEP_STR   "\\"
# define DAF_OS_SPLIT     ';'
# define DAF_OS_SPLIT_STR ";"
#else
# define DAF_OS_SEP       '/'
# define DAF_OS_SEP_STR   "/"
# define DAF_OS_SPLIT     ':'
# define DAF_OS_SPLIT_STR ":"
#endif

/* MSVC warnings */
#ifdef _MSC_VER
# define strdup _strdup
# pragma warning(disable:4244) /* possible loss of data */
#endif

/* Useful macros */
#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))
#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

#ifdef DAF_PROPER_CPP11_SUPPORT
#define CPP_AGGREGATE_INIT(ClassName) ClassName
#else
#define CPP_AGGREGATE_INIT(ClassName) (ClassName)
#endif

/* Useful typedefs */
typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;
typedef unsigned long long int ulonglong;

/* Deprecated macros */
#define DAF_DECLARE_NON_COPY_CLASS(ClassName) DAF_DECLARE_NON_COPYABLE(ClassName)
#define DAF_DECLARE_NON_COPY_STRUCT(StructName) DAF_DECLARE_NON_COPYABLE(StructName)
#define DAF_MACRO_AS_STRING(MACRO) STRINGIFY2(MACRO)

#endif // DAF_DEFINES_H_INCLUDED
