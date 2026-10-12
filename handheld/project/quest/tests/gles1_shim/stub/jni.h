// Host stand-in for <jni.h>: only the handle types the VR session header names.
#ifndef HOST_JNI_H
#define HOST_JNI_H
struct JavaVM_;
typedef JavaVM_ JavaVM;
typedef void* jobject;
#endif
