/*******************************************************************************
 * FreeRTOS+Trace v2.3.2 Recorder Library
 * Percepio AB, www.percepio.com
 *
 * trcTypes.h
 *
 * Data types used by the trace recorder library.
 *
 * Terms of Use
 * This software is copyright Percepio AB. The recorder library is free for
 * use together with Percepio products. You may distribute the recorder library
 * in its original form, including modifications in trcPort.c and trcPort.h
 * given that these modification are clearly marked as your own modifications
 * and documented in the initial comment section of these source files. 
 * This software is the intellectual property of Percepio AB and may not be 
 * sold or in other ways commercially redistributed without explicit written 
 * permission by Percepio AB.
 *
 * Disclaimer 
 * The trace tool and recorder library is being delivered to you AS IS and 
 * Percepio AB makes no warranty as to its use or performance. Percepio AB does 
 * not and cannot warrant the performance or results you may obtain by using the 
 * software or documentation. Percepio AB make no warranties, express or 
 * implied, as to noninfringement of third party rights, merchantability, or 
 * fitness for any particular purpose. In no event will Percepio AB, its 
 * technology partners, or distributors be liable to you for any consequential, 
 * incidental or special damages, including any lost profits or lost savings, 
 * even if a representative of Percepio AB has been advised of the possibility 
 * of such damages, or for any claim by any third party. Some jurisdictions do 
 * not allow the exclusion or limitation of incidental, consequential or special 
 * damages, or the exclusion of implied warranties or limitations on how long an 
 * implied warranty may last, so the above limitations may not apply to you.
 *
 * FreeRTOS+Trace is available as Free Edition and in two premium editions.
 * You may use the premium features during 30 days for evaluation.
 * Download FreeRTOS+Trace at http://www.percepio.com/products/downloads/
 *
 * Copyright Percepio AB, 2012.
 * www.percepio.com
 ******************************************************************************/

#ifndef TRCTYPES_H
#define TRCTYPES_H

#include <stdint.h>

typedef uint16_t traceLabel;

typedef uint8_t objectHandleType;

typedef uint8_t traceObjectClass;

#define TRACE_CLASS_QUEUE ((traceObjectClass)0)
#define TRACE_CLASS_SEMAPHORE ((traceObjectClass)1) 
#define TRACE_CLASS_MUTEX ((traceObjectClass)2)
#define TRACE_CLASS_TASK ((traceObjectClass)3)
#define TRACE_CLASS_ISR ((traceObjectClass)4)

typedef uint8_t traceKernelService;

#define TRACE_KERNEL_SERVICE_TASK_CREATE ((traceKernelService)0)
#define TRACE_KERNEL_SERVICE_TASK_DELETE ((traceKernelService)1)
#define TRACE_KERNEL_SERVICE_TASK_DELAY ((traceKernelService)2)
#define TRACE_KERNEL_SERVICE_PRIORITY_SET ((traceKernelService)3)
#define TRACE_KERNEL_SERVICE_TASK_SUSPEND ((traceKernelService)4)
#define TRACE_KERNEL_SERVICE_TASK_RESUME ((traceKernelService)5)
#define TRACE_KERNEL_SERVICE_QUEUE_CREATE ((traceKernelService)6)
#define TRACE_KERNEL_SERVICE_QUEUE_DELETE ((traceKernelService)7)
#define TRACE_KERNEL_SERVICE_QUEUE_SEND ((traceKernelService)8)
#define TRACE_KERNEL_SERVICE_QUEUE_RECEIVE ((traceKernelService)9)
#define TRACE_KERNEL_SERVICE_QUEUE_PEEK ((traceKernelService)10)
#define TRACE_KERNEL_SERVICE_MUTEX_CREATE ((traceKernelService)11)
#define TRACE_KERNEL_SERVICE_MUTEX_DELETE ((traceKernelService)12)
#define TRACE_KERNEL_SERVICE_MUTEX_GIVE ((traceKernelService)13)
#define TRACE_KERNEL_SERVICE_MUTEX_TAKE ((traceKernelService)14)
#define TRACE_KERNEL_SERVICE_SEMAPHORE_CREATE ((traceKernelService)15)
#define TRACE_KERNEL_SERVICE_SEMAPHORE_DELETE ((traceKernelService)16)
#define TRACE_KERNEL_SERVICE_SEMAPHORE_GIVE ((traceKernelService)17)
#define TRACE_KERNEL_SERVICE_SEMAPHORE_TAKE ((traceKernelService)18)

#endif
