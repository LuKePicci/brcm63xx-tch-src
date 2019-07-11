/*
    Copyright 2007-2015 Broadcom Corporation

    <:label-BRCM:2015:DUAL/GPL:standard
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation (the "GPL").
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    
    A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
    writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
    
    :>
*/

//
// bcmtypes.h - misc useful typedefs
//
#ifndef BCMTYPES_H
#define BCMTYPES_H

// These are also defined in typedefs.h in the application area, so I need to
// protect against re-definition.

#ifndef _TYPEDEFS_H_

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;
typedef unsigned long long  uint64;
typedef signed char     int8;
typedef signed short    int16;
typedef signed int     int32;
typedef signed long long    int64;
typedef unsigned long  BcmHandle;

#if !defined(__cplusplus) && !defined(__KERNEL__) && !defined(_LINUX_TYPES_H) && !defined(_LINUX_IF_H)
#ifndef bool
typedef int bool;
#endif
#endif

#endif

typedef unsigned char  byte;

// typedef unsigned long   sem_t;

typedef unsigned long   HANDLE,*PULONG;
typedef int             DWORD,*PDWORD;
#ifndef LONG
typedef signed long     LONG,*PLONG;
#endif

typedef unsigned int    *PUINT;
typedef signed int      INT;

typedef unsigned short  *PUSHORT;
typedef signed short    SHORT,*PSHORT;
typedef unsigned short  WORD,*PWORD;

typedef unsigned char   *PUCHAR;
typedef signed char     *PCHAR;

typedef void            *PVOID;

typedef unsigned char   BOOLEAN, *PBOOL, *PBOOLEAN;

typedef unsigned char   BYTE,*PBYTE;

typedef signed int      *PINT;

#ifndef NUMBER_TYPES_ALREADY_DEFINED
#define NUMBER_TYPES_ALREADY_DEFINED
typedef signed char     INT8;
typedef signed char     SINT8;
typedef signed short    INT16;
typedef signed short    SINT16;
typedef signed int      INT32;
typedef signed int      sint32;
typedef signed int      SINT32;
typedef signed long long SINT64;

typedef unsigned char   UINT8;
typedef unsigned short  UINT16;
typedef unsigned int    UINT32;
typedef unsigned long long UINT64;
#endif

#ifndef BOOL_TYPE_ALREADY_DEFINED
#define BOOL_TYPE_ALREADY_DEFINED
typedef unsigned char   UBOOL8;
#endif

#ifndef BASE_TYPE_ALREADY_DEFINED
#define BASE_TYPE_ALREADY_DEFINED
typedef unsigned char   UCHAR;
typedef unsigned short  USHORT;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
#endif

typedef void            VOID;
typedef unsigned char   BOOL;

// These are also defined in typedefs.h in the application area, so I need to
// protect against re-definition.
#ifndef TYPEDEFS_H

// Maximum and minimum values for a signed 16 bit integer.
#define MAX_INT16 32767
#define MIN_INT16 -32768

// Useful for true/false return values.  This uses the
// Taligent notation (k for constant).
typedef enum
{
    kFalse = 0,
    kTrue = 1
} Bool;

#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE  0
#endif

#define READ32(addr)        (*(volatile UINT32 *)((ULONG)&addr))
#define READ16(addr)        (*(volatile UINT16 *)((ULONG)&addr))
#define READ8(addr)         (*(volatile UINT8  *)((ULONG)&addr))

#define VIRT_TO_PHY(a)  (((unsigned long)(a)) & 0x1fffffff)

typedef unsigned long long uint64_aligned __attribute__((aligned(8)));
typedef signed long long int64_aligned __attribute__((aligned(8)));

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BCM_IOC_PTR(ptr_t, ptr) union { ptr_t ptr; uint64_aligned ptr##64; }
#define BCM_IOC_PTR_ZERO_EXT(ptr) ((ptr##64) = (uint64_aligned)(uint32_t)(ptr##64))
#else
#define BCM_IOC_PTR(ptr_t, ptr) ptr_t ptr;
#define BCM_IOC_PTR_ZERO_EXT(ptr)
#endif

/*Example usage of above types/macro to a create 32/64-bit compatible ioctl message:
  typedef struct {
    uint8 exByte;
    BCM_IOC_PTR(void*, exPtr);
    uint32 exWord; /.Never use long. Always use (u)int32./
    uint64_aligned ex64bitValue; /.Never use long long. Always use (u)int64_aligned../
  } ExampleIoctlMsg;
*/
#endif
