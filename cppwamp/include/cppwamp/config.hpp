/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONFIG_HPP
#define CPPWAMP_CONFIG_HPP

//------------------------------------------------------------------------------
// Target system detection
//------------------------------------------------------------------------------
#if defined(_WIN32) || defined(__CYGWIN__)
#   define CPPWAMP_SYSTEM_IS_WINDOWS
#   define CPPWAMP_SYSTEM_NAME "Windows"
#elif defined(__APPLE__)
#   include <TargetConditionals.h>
#   if defined(TARGET_IPHONE_SIMULATOR) || defined(TARGET_OS_IPHONE)
#       define CPPWAMP_SYSTEM_IS_IOS 1
#       define CPPWAMP_SYSTEM_NAME "iOS"
#   elif defined(TARGET_OS_MAC) || defined(TARGET_OS_MACCATALYST)
#       define CPPWAMP_SYSTEM_IS_MACOS 1
#       define CPPWAMP_SYSTEM_NAME "macOS"
#   else
#       define CPPWAMP_SYSTEM_IS_APPLE 1
#       define CPPWAMP_SYSTEM_NAME "Apple"
#   endif
#elif defined(__ANDROID__)
#   define CPPWAMP_SYSTEM_IS_ANDROID 1
#   define CPPWAMP_SYSTEM_NAME "Android"
#elif defined(__linux__)
#   define CPPWAMP_SYSTEM_IS_LINUX 1
#   define CPPWAMP_SYSTEM_NAME "Linux"
#elif defined(BSD)
#   define CPPWAMP_SYSTEM_IS_BSD 1
#   define CPPWAMP_SYSTEM_NAME "BSD"
#elif defined(__unix__)
#   define CPPWAMP_SYSTEM_IS_UNIX 1
#   define CPPWAMP_SYSTEM_NAME "UNIX"
#elif defined(_POSIX_VERSION)
#   define CPPWAMP_SYSTEM_IS_POSIX 1
#   define CPPWAMP_SYSTEM_NAME "POSIX"
#else
#   define CPPWAMP_SYSTEM_IS_UNDETECTED 1
#   define CPPWAMP_SYSTEM_NAME "Undetected"
#endif

#ifdef CPPWAMP_CUSTOM_SYSTEM_NAME
#undef CPPWAMP_SYSTEM_NAME
#define CPPWAMP_SYSTEM_NAME CPPWAMP_CUSTOM_SYSTEM_NAME
#endif

//------------------------------------------------------------------------------
// Target architecture detection
//------------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
#   define CPPWAMP_ARCH_IS_X86_64 1
#   define CPPWAMP_ARCH_NAME "x86-64"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#   define CPPWAMP_ARCH_IS_X86_32 1
#   define CPPWAMP_ARCH_NAME "x86"
#elif defined(__ia64__) || defined(_M_IA64)
#   define CPPWAMP_ARCH_IS_ITANIUM 1
#   define CPPWAMP_ARCH_NAME "IA-64"
#elif defined(__arm__) || defined(_M_ARM)
#   define CPPWAMP_ARCH_IS_ARM 1
#   define CPPWAMP_ARCH_NAME "ARM"
#elif defined(__aarch64__) || defined(_M_ARM64)
#   define CPPWAMP_ARCH_IS_ARM64 1
#   define CPPWAMP_ARCH_NAME "ARM64"
#elif defined(mips) || defined(__mips__) || defined(__mips)
#   define CPPWAMP_ARCH_IS_MIPS 1
#   define CPPWAMP_ARCH_NAME "MIPS"
#elif defined(__sh__)
#   define CPPWAMP_ARCH_IS_SUPERH 1
#   define CPPWAMP_ARCH_NAME "SuperH"
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || \
      defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || \
      defined(_ARCH_PPC)
#   define CPPWAMP_ARCH_IS_POWERPC 1
#   define CPPWAMP_ARCH_NAME "PowerPC"
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#   define CPPWAMP_ARCH_IS_POWERPC64 1
#   define CPPWAMP_ARCH_NAME "PPC64"
#elif defined(__sparc__) || defined(__sparc)
#   define CPPWAMP_ARCH_IS_SPARC 1
#   define CPPWAMP_ARCH_NAME "SPARC"
#elif defined(__riscv)
#   define CPPWAMP_ARCH_IS_RISCV 1
#   define CPPWAMP_ARCH_NAME "RISC-V"
#else
#   define CPPWAMP_ARCH_IS_UNDETECTED 1
#   define CPPWAMP_ARCH_NAME "Undetected"
#endif

#ifdef CPPWAMP_CUSTOM_ARCH_NAME
#undef CPPWAMP_ARCH_NAME
#define CPPWAMP_ARCH_NAME CPPWAMP_CUSTOM_ARCH_NAME
#endif

//------------------------------------------------------------------------------
#ifndef CPPWAMP_SYSTEM_IS_WINDOWS
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 1
#endif

//------------------------------------------------------------------------------
#if defined(__cpp_inline_variables) || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_INLINE_VARIABLE inline
#else
#define CPPWAMP_INLINE_VARIABLE
#endif

//------------------------------------------------------------------------------
#if (defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)) \
    || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_NODISCARD [[nodiscard]]
#else
#define CPPWAMP_NODISCARD
#endif

//------------------------------------------------------------------------------
#if (defined(__has_cpp_attribute) && __has_cpp_attribute(deprecated)) \
    || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_DEPRECATED [[deprecated]]
#else
#define CPPWAMP_DEPRECATED
#endif

//------------------------------------------------------------------------------
#if (defined(__cpp_constexpr) && (__cpp_constexpr >= 201304)) \
    || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_HAS_RELAXED_CONSTEXPR 1
#define CPPWAMP_CONSTEXPR14 constexpr
#else
#define CPPWAMP_CONSTEXPR14
#endif

#endif // CPPWAMP_CONFIG_HPP
