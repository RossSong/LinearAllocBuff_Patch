#include<assert.h>

struct Object;
struct ClassObject;
struct Method;
struct ClassPathEntry;
struct DvmDex;
struct HashTable;
struct InitiatingLoaderList;
struct Thread;
struct BitVector;
struct ReferenceTable;
struct Monitor;
struct InstructionWidth;
struct InstructionFlags;
struct InstructionFormat;
struct AtomicCache;
struct JdwpState;
struct StepControl;
struct AllocRecord;
struct MethodTraceState;
struct AllocProfState;

#ifndef _DALVIK_GEN_INLINES             /* only defined by Inlines.c */
# define INLINE extern __inline__
#else
# define INLINE
#endif

/*
 * These match the definitions in the VM specification.
 */
#ifdef HAVE_STDINT_H
# include <stdint.h>    /* C99 */
typedef uint8_t             u1;
typedef uint16_t            u2;
typedef uint32_t            u4;
typedef uint64_t            u8;
typedef int8_t              s1;
typedef int16_t             s2;
typedef int32_t             s4;
typedef int64_t             s8;
#else
typedef unsigned char       u1;
typedef unsigned short      u2;
typedef unsigned int        u4;
typedef unsigned long long  u8;
typedef signed char         s1;
typedef signed short        s2;
typedef signed int          s4;
typedef signed long long    s8;
#endif

/*
 * Global DEX optimizer control.  Determines the circumstances in which we
 * try to rewrite instructions in the DEX file.
 */
typedef enum DexOptimizerMode {
    OPTIMIZE_MODE_UNKNOWN = 0,
    OPTIMIZE_MODE_NONE,         /* never optimize */
    OPTIMIZE_MODE_VERIFIED,     /* only optimize verified classes (default) */
    OPTIMIZE_MODE_ALL           /* optimize all classes */
} DexOptimizerMode;


/*
 * Global verification mode.  These must be in order from least verification
 * to most.  If we're using "exact GC", we may need to perform some of
 * the verification steps anyway.
 */
typedef enum {
    VERIFY_MODE_UNKNOWN = 0,
    VERIFY_MODE_NONE,
    VERIFY_MODE_REMOTE,
    VERIFY_MODE_ALL
} DexClassVerifyMode;

/*
 * Primitive type identifiers.  We use these values as indexes into an
 * array of synthesized classes, so these start at zero and count up.
 * The order is arbitrary (mimics table in doc for newarray opcode),
 * but can't be changed without shuffling some reflection tables.
 *
 * PRIM_VOID can't be used as an array type, but we include it here for
 * other uses (e.g. Void.TYPE).
 */
typedef enum PrimitiveType {
    PRIM_NOT        = -1,       /* value is not a primitive type */
    PRIM_BOOLEAN    = 0,
    PRIM_CHAR       = 1,
    PRIM_FLOAT      = 2,
    PRIM_DOUBLE     = 3,
    PRIM_BYTE       = 4,
    PRIM_SHORT      = 5,
    PRIM_INT        = 6,
    PRIM_LONG       = 7,
    PRIM_VOID       = 8,

    PRIM_MAX
} PrimitiveType;

/*
 * Linear allocation state.  We could tuck this into the start of the
 * allocated region, but that would prevent us from sharing the rest of
 * that first page.
 */
typedef struct LinearAllocHdr {
    int     curOffset;          /* offset where next data goes */
    pthread_mutex_t lock;       /* controls updates to this struct */

    char*   mapAddr;            /* start of mmap()ed region */
    int     mapLength;          /* length of region */
    int     firstOffset;        /* for chasing through */

    short*  writeRefCount;      /* for ENFORCE_READ_ONLY */
} LinearAllocHdr;

/*
 * Indirect reference kind, used as the two low bits of IndirectRef.
 *
 * For convenience these match up with enum jobjectRefType from jni.h.
 */
typedef enum IndirectRefKind {
    kIndirectKindInvalid    = 0,
    kIndirectKindLocal      = 1,
    kIndirectKindGlobal     = 2,
    kIndirectKindWeakGlobal = 3
} IndirectRefKind;

/*
 * Extended debugging structure.  We keep a parallel array of these, one
 * per slot in the table.
 */
#define kIRTPrevCount   4
typedef struct IndirectRefSlot {
    u4          serial;         /* slot serial */
    Object*     previous[kIRTPrevCount];
} IndirectRefSlot;



typedef union IRTSegmentState {
    u4          all;
    struct {
        u4      topIndex:16;            /* index of first unused entry */
        u4      numHoles:16;            /* #of holes in entire table */
    } parts;
} IRTSegmentState;

typedef struct IndirectRefTable {
    /* semi-public - read/write by interpreter in native call handler */
    IRTSegmentState segmentState;

    /* semi-public - read-only during GC scan; pointer must not be kept */
    Object**        table;              /* bottom of the stack */

    /* private */
    IndirectRefSlot* slotData;          /* extended debugging info */
    int             allocEntries;       /* #of entries we have space for */
    int             maxEntries;         /* max #of entries allowed */
    IndirectRefKind kind;               /* bit mask, ORed into all irefs */

    // TODO: want hole-filling stats (#of holes filled, total entries scanned)
    //       for performance evaluation.
} IndirectRefTable;

/*
 * Method trace state.  This is currently global.  In theory we could make
 * most of this per-thread.
 */
typedef struct MethodTraceState {
    /* these are set during VM init */
    Method* gcMethod;
    Method* classPrepMethod;

    /* active state */
    pthread_mutex_t startStopLock;
    pthread_cond_t  threadExitCond;
    FILE*   traceFile;
    bool    directToDdms;
    int     bufferSize;
    int     flags;

    int     traceEnabled;
    u1*     buf;
    volatile int curOffset;
    u8      startWhen;
    int     overflow;
} MethodTraceState;

/*
 * Memory allocation profiler state.  This is used both globally and
 * per-thread.
 *
 * If you add a field here, zero it out in dvmStartAllocCounting().
 */
typedef struct AllocProfState {
    bool    enabled;            // is allocation tracking enabled?

    int     allocCount;         // #of objects allocated
    int     allocSize;          // cumulative size of objects

    int     failedAllocCount;   // #of times an allocation failed
    int     failedAllocSize;    // cumulative size of failed allocations

    int     freeCount;          // #of objects freed
    int     freeSize;           // cumulative size of freed objects

    int     gcCount;            // #of times an allocation triggered a GC

    int     classInitCount;     // #of initialized classes
    u8      classInitTime;      // cumulative time spent in class init (nsec)

#if PROFILE_EXTERNAL_ALLOCATIONS
    int     externalAllocCount; // #of calls to dvmTrackExternalAllocation()
    int     externalAllocSize;  // #of bytes passed to ...ExternalAllocation()

    int     failedExternalAllocCount; // #of times an allocation failed
    int     failedExternalAllocSize;  // cumulative size of failed allocations

    int     externalFreeCount;  // #of calls to dvmTrackExternalFree()
    int     externalFreeSize;   // #of bytes passed to ...ExternalFree()
#endif  // PROFILE_EXTERNAL_ALLOCATIONS
} AllocProfState;

/*
 * Table definition.
 *
 * The expected common operations are adding a new entry and removing a
 * recently-added entry (usually the most-recently-added entry).
 *
 * If "allocEntries" is not equal to "maxEntries", the table may expand when
 * entries are added, which means the memory may move.  If you want to keep
 * pointers into "table" rather than offsets, use a fixed-size table.
 *
 * (This structure is still somewhat transparent; direct access to
 * table/nextEntry is allowed.)
 */
typedef struct ReferenceTable {
    Object**        nextEntry;          /* top of the list */
    Object**        table;              /* bottom of the list */

    int             allocEntries;       /* #of entries we have space for */
    int             maxEntries;         /* max #of entries allowed */
} ReferenceTable;

/*
 * StepDepth constants.
 */
enum JdwpStepDepth {
    SD_INTO                 = 0,    /* step into method calls */
    SD_OVER                 = 1,    /* step over method calls */
    SD_OUT                  = 2,    /* step out of current method */
};

/*
 * StepSize constants.
 */
enum JdwpStepSize {
    SS_MIN                  = 0,    /* step by minimum (e.g. 1 bytecode inst) */
    SS_LINE                 = 1,    /* if possible, step to next line */
};

/*
 * Used by StepControl to track a set of addresses associated with
 * a single line.
 */
typedef struct AddressSet {
    u4 setSize;
    u1 set[1];
} AddressSet;

/*
 * Single-step management.
 */
typedef struct StepControl {
    /* request */
    enum JdwpStepSize   size;
    enum JdwpStepDepth  depth;
    struct Thread*      thread;         /* don't deref; for comparison only */

    /* current state */
    bool                active;
    const struct Method* method;
    int                 line;           /* line #; could be -1 */
    const AddressSet*   pAddressSet;    /* if non-null, address set for line */
    int                 frameDepth;
} StepControl;
