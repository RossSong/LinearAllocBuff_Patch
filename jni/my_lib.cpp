/*
 *  Collin's Dynamic Dalvik Instrumentation Toolkit for Android
 *  Collin Mulliner <collin[at]mulliner.org>
 *
 *  (c) 2012,2013
 *
 *  License: LGPL v2.1
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <android/log.h>

#include <jni.h>
#include <stdlib.h>

#include <dlfcn.h>
#include "extra.h"
#include "Globals.h"
#include <sys/mman.h>
#include <errno.h>

#undef log

#define log(s, ...) \
__android_log_print(ANDROID_LOG_INFO, "MyTag", s, __VA_ARGS__);\
{FILE *fp = fopen("/mnt/sdcard/strmon.log", "a+");\
if(NULL != fp){ \
fprintf(fp, s, __VA_ARGS__);\
fclose(fp);\
}\
}

#define DEFAULT_MAX_LENGTH  (16*1024*1024) //!!!!!!!!!!!! - 16MB

#define ALOGW(...) printf("W/" __VA_ARGS__)
#define ALOGE(...) printf("E/" __VA_ARGS__)
#define ALOGV(...) printf("W/" __VA_ARGS__)
#define ENFORCE_READ_ONLY   false


/*
 * Initialize a mutex.
 */
INLINE void dvmInitMutex(pthread_mutex_t* pMutex)
{
#ifdef CHECK_MUTEX
    pthread_mutexattr_t attr;
    int cc;

    pthread_mutexattr_init(&attr);
    cc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    assert(cc == 0);
    pthread_mutex_init(pMutex, &attr);
    pthread_mutexattr_destroy(&attr);
#else
    pthread_mutex_init(pMutex, NULL);       // default=PTHREAD_MUTEX_FAST_NP
#endif
}

/*
 * Grab a plain mutex.
 */
INLINE void dvmLockMutex(pthread_mutex_t* pMutex)
{
    int cc __attribute__ ((__unused__)) = pthread_mutex_lock(pMutex);
    // ITB_TODO: initialize this elsewhere
    if (cc == EINVAL){
        dvmInitMutex(pMutex);
        cc = pthread_mutex_lock(pMutex);
    }
    assert(cc == 0);
}

/*
 * Try grabbing a plain mutex.  Returns 0 if successful.
 */
INLINE int dvmTryLockMutex(pthread_mutex_t* pMutex)
{
    int cc = pthread_mutex_trylock(pMutex);
    assert(cc == 0 || cc == EBUSY);
    return cc;
}

/*
 * Unlock pthread mutex.
 */
INLINE void dvmUnlockMutex(pthread_mutex_t* pMutex)
{
    int cc __attribute__ ((__unused__)) = pthread_mutex_unlock(pMutex);
    assert(cc == 0);
}

#include <sys/types.h>

void SwapLinearAllocBuffer(LinearAllocHdr *pHdr)
{
    char buff[1024] = {0x00,};

    if( NULL == pHdr ){ log("%s", "pHdr is NULL"); return; }

    int fd;

    log("%s", "Swap Start");//

    dvmLockMutex(&pHdr->lock);

    log("%s", "Lock");//

    void *oldPtr = pHdr->mapAddr;
    pHdr->mapAddr = (char*)mmap(NULL, DEFAULT_MAX_LENGTH, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    snprintf(buff, 1024, "mmap pHdr->mapAddr: 0x%x", pHdr->mapAddr);//
    log("%s", buff);

    if (mprotect(pHdr->mapAddr, DEFAULT_MAX_LENGTH, PROT_READ | PROT_WRITE) != 0) {
        log("%s", "LinearAlloc init mprotect failed: %s", strerror(errno));
        return ;
    }

    log("%s", "mprotect");//

    close(fd);

    memset(pHdr->mapAddr, 0, DEFAULT_MAX_LENGTH);

    log("%s", "memset");//
    if(mprotect(oldPtr, pHdr->mapLength, PROT_READ | PROT_WRITE) != 0 )
    {
        log("%s", "oldPtr mprotect failed: %s", strerror(errno));
	return;
    }

    memcpy(pHdr->mapAddr, oldPtr, pHdr->mapLength);

    log("%s", "memcpy");//
 
    if (munmap(oldPtr, pHdr->mapLength) != 0) {
        ALOGW("LinearAlloc munmap(%p, %d) failed: %s",
            oldPtr, pHdr->mapLength, strerror(errno));
    }

    log("%s", "oldPtr munmap\n");//

    pHdr->mapLength = DEFAULT_MAX_LENGTH;
    dvmUnlockMutex(&pHdr->lock);

    log("%s", "UnLock\n");//
}


jint JNI_OnLoad(JavaVM * vm, void* reserved)
{
	char buff[1024] = {0x00,};
	struct DvmGlobals *t_gDvm = NULL;

	void * handle = dlopen("libdvm.so", RTLD_NOW);
	snprintf(buff, 1024, "handle is 0x%x", handle);
	log("%s", buff);
	
	//DvmGlobals 구조체를 얻는다.
	t_gDvm = (struct DvmGlobals *)dlsym(handle, "gDvm");

	if( t_gDvm )
	{
		if( NULL == t_gDvm->pBootLoaderAlloc )
			return -1;

		snprintf(buff, 1024, "t_gDvm: %p , addr: %p t_gDvm->pBootLoaderAlloc: 0x%x", t_gDvm, &(t_gDvm->pBootLoaderAlloc), t_gDvm->pBootLoaderAlloc);
		log("%s", buff);
		snprintf(buff, 1024, "mapAddr: %p mapLength: 0x%x", t_gDvm->pBootLoaderAlloc->mapAddr, t_gDvm->pBootLoaderAlloc->mapLength);
		log("%s", buff);

		log("%s", "Try to change LinearAlloc buffer.");

		//Change Linear Alloc Buffer!!
		SwapLinearAllocBuffer(t_gDvm->pBootLoaderAlloc);

		log("%s", "Success to change LinearAlloc Buffer.");
		snprintf(buff, 1024, "mapAddr: %p mapLength: 0x%x", t_gDvm->pBootLoaderAlloc->mapAddr, t_gDvm->pBootLoaderAlloc->mapLength);
		log("%s", buff);
	}
	else
	{
		log("%s", "Error!! - t_gDvm is NULL");
	}

	JNIEnv* env;
	if( vm->GetEnv((void**)&env, JNI_VERSION_1_6 ) != JNI_OK)
		return -1;
	
	log("%s", "JNI INIT");
	return JNI_VERSION_1_6;
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL Java_com_example_ndktest_NativeCall_stringFromJNI(JNIEnv *env, jobject obj, jobject obj_Context)
{
    return env->NewStringUTF("Hello JNI!!!!!");
}

JNIEXPORT jint JNICALL Java_com_example_ndktest_NativeCall_add(JNIEnv *env, jobject obj, jint i, jint j)
{
    return i + j;
}

#ifdef __cplusplus
}
#endif
