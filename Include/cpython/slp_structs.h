#ifndef STACKLESS_STRUCTS_H
#define STACKLESS_STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STACKLESS

#if defined(Py_BUILD_CORE) && !defined(SLP_BUILD_CORE)
#define SLP_BUILD_CORE
#endif

#ifdef SLP_BUILD_CORE

#if defined(MS_WIN32) && !defined(MS_WIN64) && defined(_M_IX86)
#define SLP_SEH32
#endif

/*** important structures: tasklet ***/


/***************************************************************************

    Tasklet Flag Definition
    -----------------------

    blocked:        The tasklet is either waiting in a channel for
                    writing (1) or reading (-1) or not blocked (0).
                    Maintained by the channel logic. Do not change.

    atomic:         If true, schedulers will never switch. Driven by
                    the code object or dynamically, see below.

    ignore_nesting: Allows auto-scheduling, even if nesting_level
                    is not zero.

    autoschedule:   The tasklet likes to be auto-scheduled. User driven.

    block_trap:     Debugging aid. Whenever the tasklet would be
                    blocked by a channel, an exception is raised.

    is_zombie:      This tasklet is almost dead, its deallocation has
                    started. The tasklet *must* die at some time, or the
                    process can never end.

    pending_irq:    If set, an interrupt was issued during an atomic
                    operation, and should be handled when possible.


    Policy for atomic/autoschedule and switching:
    ---------------------------------------------
    A tasklet switch can always be done explicitly by calling schedule().
    Atomic and schedule are concerned with automatic features.

    atomic  autoschedule

    1           any     Neither a scheduler nor a watchdog will
                        try to switch this tasklet.

    0           0       The tasklet can be stopped on desire, or it
                        can be killed by an exception.

    0           1       Like above, plus auto-scheduling is enabled.

    Default settings:
    -----------------
    All flags are zero by default.

 ***************************************************************************/

#define SLP_TASKLET_FLAGS_BITS_blocked 2
#define SLP_TASKLET_FLAGS_BITS_atomic 1
#define SLP_TASKLET_FLAGS_BITS_ignore_nesting 1
#define SLP_TASKLET_FLAGS_BITS_autoschedule 1
#define SLP_TASKLET_FLAGS_BITS_block_trap 1
#define SLP_TASKLET_FLAGS_BITS_is_zombie 1
#define SLP_TASKLET_FLAGS_BITS_pending_irq 1

#define SLP_TASKLET_FLAGS_OFFSET_blocked 0
#define SLP_TASKLET_FLAGS_OFFSET_atomic \
    (SLP_TASKLET_FLAGS_OFFSET_blocked + SLP_TASKLET_FLAGS_BITS_blocked)
#define SLP_TASKLET_FLAGS_OFFSET_ignore_nesting \
    (SLP_TASKLET_FLAGS_OFFSET_atomic + SLP_TASKLET_FLAGS_BITS_atomic)
#define SLP_TASKLET_FLAGS_OFFSET_autoschedule \
    (SLP_TASKLET_FLAGS_OFFSET_ignore_nesting + SLP_TASKLET_FLAGS_BITS_ignore_nesting)
#define SLP_TASKLET_FLAGS_OFFSET_block_trap \
    (SLP_TASKLET_FLAGS_OFFSET_autoschedule + SLP_TASKLET_FLAGS_BITS_autoschedule)
#define SLP_TASKLET_FLAGS_OFFSET_is_zombie \
    (SLP_TASKLET_FLAGS_OFFSET_block_trap + SLP_TASKLET_FLAGS_BITS_block_trap)
#define SLP_TASKLET_FLAGS_OFFSET_pending_irq \
    (SLP_TASKLET_FLAGS_OFFSET_is_zombie + SLP_TASKLET_FLAGS_BITS_is_zombie)

typedef struct _slp_tasklet_flags {
    signed int blocked: SLP_TASKLET_FLAGS_BITS_blocked;
    unsigned int atomic: SLP_TASKLET_FLAGS_BITS_atomic;
    unsigned int ignore_nesting: SLP_TASKLET_FLAGS_BITS_ignore_nesting;
    unsigned int autoschedule: SLP_TASKLET_FLAGS_BITS_autoschedule;
    unsigned int block_trap: SLP_TASKLET_FLAGS_BITS_block_trap;
    unsigned int is_zombie: SLP_TASKLET_FLAGS_BITS_is_zombie;
    unsigned int pending_irq: SLP_TASKLET_FLAGS_BITS_pending_irq;
} PyTaskletFlagStruc;

typedef struct _slp_tasklet {
    PyObject_HEAD
    struct _slp_tasklet *next;
    struct _slp_tasklet *prev;
    union {
        struct _frame *frame;
        struct _cframe *cframe;
    } f;
    PyObject *tempval;
    struct _slp_cstack *cstate;
    /* Pointer to the top of the stack of the exceptions currently
     * being handled */
    _PyErr_StackItem *exc_info;
    /* The exception currently being handled, if no coroutines/generators
     * are present. Always last element on the stack referred to be exc_info.
     */
    _PyErr_StackItem exc_state;
    /* bits stuff */
    struct _slp_tasklet_flags flags;
    int recursion_depth;
    PyObject *def_globals;
    PyObject *tsk_weakreflist;
    /* If the tasklet is current: NULL. (The context of a current tasklet is
     *  always in ts->context.)
     * If the tasklet is not current: the context for the tasklet */
    PyObject *context;

    /* If the tasklet is current: NULL. (The profile and tracing state of a
     *  current tasklet is stored in the thread state.)
     * If the tasklet is not current: the profile and tracing state for the tasklet.
     */
    Py_tracefunc profilefunc;
    Py_tracefunc tracefunc;
    PyObject *profileobj;
    PyObject *traceobj;
    int tracing;
} PyTaskletObject;


/*** important structures: cstack ***/

typedef struct _slp_cstack {
    PyObject_VAR_HEAD
    struct _slp_cstack *next;
    struct _slp_cstack *prev;
    PY_LONG_LONG serial;
    /* A borrowed reference to the tasklet, that owns this cstack. NULL after
     * the stack has been restored. Always NULL for an initial stub.
     */
    struct _slp_tasklet *task;
    int nesting_level;
    PyThreadState *tstate;
#ifdef SLP_SEH32
        /* SEH handler on Win32
         * The correct type is DWORD, but we do not want to include <windows.h>.
         * Instead we use a compile time assertion to ensure, that we use an
         * equivalent type.
         */
    unsigned long exception_list;
#endif
    /* The end-address (sic!) of the stack stored in the cstack.
     */
    intptr_t *startaddr;
    intptr_t stack[
#if defined(_WINDOWS_) && defined(SLP_SEH32)
                /* Assert the equivalence of DWORD and unsigned long. If <windows.h>
                 * is included, _WINDOWS_ is defined.
                 * Py_BUILD_ASSERT_EXPR needs an expression and this is the only one.
                 */
                Py_BUILD_ASSERT_EXPR(sizeof(unsigned long) == sizeof(DWORD)) +
                Py_BUILD_ASSERT_EXPR(((DWORD)-1) > 0) + /* signedness */
#endif
                1];
} PyCStackObject;


/*** important structures: bomb ***/

typedef struct _slp_bomb {
    PyObject_HEAD
    PyObject *curexc_type;
    PyObject *curexc_value;
    PyObject *curexc_traceback;
} PyBombObject;

/*** important structures: channel ***/

/***************************************************************************

    Channel Flag Definition
    -----------------------


    closing:        When the closing flag is set, the channel does not
                    accept to be extended. The computed attribute
                    'closed' is true when closing is set and the
                    channel is empty.

    preference:     0    no preference, caller will continue
                    1    sender will be inserted after receiver and run
                    -1   receiver will be inserted after sender and run

    schedule_all:   ignore preference and always schedule the next task

    Default settings:
    -----------------
    All flags are zero by default.

 ***************************************************************************/

#define SLP_CHANNEL_FLAGS_BITS_closing 1
#define SLP_CHANNEL_FLAGS_BITS_preference 2
#define SLP_CHANNEL_FLAGS_BITS_schedule_all 1

#define SLP_CHANNEL_FLAGS_OFFSET_closing 0
#define SLP_CHANNEL_FLAGS_OFFSET_preference \
    (SLP_CHANNEL_FLAGS_OFFSET_closing + SLP_CHANNEL_FLAGS_BITS_closing)
#define SLP_CHANNEL_FLAGS_OFFSET_schedule_all \
    (SLP_CHANNEL_FLAGS_OFFSET_preference + SLP_CHANNEL_FLAGS_BITS_preference)

typedef struct _slp_channel_flags {
    unsigned int closing: SLP_CHANNEL_FLAGS_BITS_closing;
    signed int preference: SLP_CHANNEL_FLAGS_BITS_preference;
    unsigned int schedule_all: SLP_CHANNEL_FLAGS_BITS_schedule_all;
} PyChannelFlagStruc;

typedef struct _slp_channel {
    PyObject_HEAD
    /* make sure that these fit tasklet's next/prev */
    struct _slp_tasklet *head;
    struct _slp_tasklet *tail;
    int balance;
    struct _slp_channel_flags flags;
    PyObject *chan_weakreflist;
} PyChannelObject;

struct _slp_cframe;
typedef PyObject *(PyFrame_ExecFunc) (struct _slp_cframe *, int, PyObject *);
/*
 * How to write frame execution functions:
 *
 * Special rule for frame execution functions: the function owns a reference to retval!
 *
 *  PyObject * example(PyCFrameObject *f, int exc, PyObject *retval)
 *  {
 *     PyThreadState *ts = _PyThreadState_GET();
 *
 *     do something ....
 *     If you change retval, use Py_SETREF(retval, new_value) or Py_CLEAR(retval).
 *
 *     SLP_STORE_NEXT_FRAME(ts, f->f_back);
 *     return retval;
 *  }
 *
 */

/*** important stuctures: cframe ***/

typedef struct _slp_cframe {
    PyObject_VAR_HEAD
    struct _frame *f_back;      /* previous frame, or NULL */

    /*
     * the above part is compatible with regular frames.
     */

    PyFrame_ExecFunc *f_execute;

    /* these can be used as the CFrame likes to */
    PyObject *ob1;
    PyObject *ob2;
    PyObject *ob3;
    long i, n;
    void *any1;
    void *any2;
} PyCFrameObject;

typedef struct _slp_unwindobject {
    PyObject_HEAD
} PyUnwindObject;


#else /* #ifdef SLP_BUILD_CORE */

typedef struct _slp_channel PyChannelObject;
typedef struct _slp_cframe  PyCFrameObject;
typedef struct _slp_tasklet PyTaskletObject;
typedef struct _slp_unwindobject PyUnwindObject;
typedef struct _slp_bomb PyBombObject;

#endif /* #ifdef SLP_BUILD_CORE */

/*** associated type objects ***/

PyAPI_DATA(PyTypeObject) PyCFrame_Type;
#define PyCFrame_Check(op) (Py_TYPE(op) == &PyCFrame_Type)

PyAPI_DATA(PyTypeObject) PyCStack_Type;
#define PyCStack_Check(op) (Py_TYPE(op) == &PyCStack_Type)

PyAPI_DATA(PyTypeObject) PyBomb_Type;
#define PyBomb_Check(op) (Py_TYPE(op) == &PyBomb_Type)

PyAPI_DATA(PyTypeObject) PyTasklet_Type;
#define PyTasklet_Check(op) PyObject_TypeCheck(op, &PyTasklet_Type)
#define PyTasklet_CheckExact(op) (Py_TYPE(op) == &PyTasklet_Type)

PyAPI_DATA(PyTypeObject) PyChannel_Type;
#define PyChannel_Check(op) PyObject_TypeCheck(op, &PyChannel_Type)
#define PyChannel_CheckExact(op) (Py_TYPE(op) == &PyChannel_Type)

/******************************************************
  Macros for the stackless protocol
 ******************************************************/

#ifndef _PyStackless_TRY_STACKLESS
PyAPI_DATA(intptr_t * const) _PyStackless__TryStacklessPtr;
#define _PyStackless_TRY_STACKLESS (*_PyStackless__TryStacklessPtr)
#endif
#ifndef STACKLESS__GETARG_ASSERT
#define STACKLESS__GETARG_ASSERT ((void)0)
#endif

#endif /* #ifdef STACKLESS */

#ifdef __cplusplus
}
#endif

#endif
