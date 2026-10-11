#include "bridge/mapping.hpp"
#include <jni.h>
#include <optional>
#include <thread>
namespace {
std::optional<steve::Mapping> mapping;
bool input_owned=false, output_owned=false;
void fail(JNIEnv* env,const char* message) {
    auto type=env->FindClass("java/lang/IllegalStateException");
    if(type) env->ThrowNew(type,message);
}
}
extern "C" {
JNIEXPORT jboolean JNICALL Java_org_ocarina_bridge_NativeBridge_open(JNIEnv* env,jclass,jstring session) {
    if(mapping) return JNI_TRUE;
    const char* name=env->GetStringUTFChars(session,nullptr);
    if(!name) return JNI_FALSE;
    auto opened=steve::Mapping::open(name,false);
    env->ReleaseStringUTFChars(session,name);
    if(!opened) return JNI_FALSE;
    mapping.emplace(std::move(*opened)); return JNI_TRUE;
}
JNIEXPORT jobject JNICALL Java_org_ocarina_bridge_NativeBridge_peek(JNIEnv* env,jclass) {
    if(!mapping || input_owned) { fail(env,"input slot ownership violation"); return nullptr; }
    auto* packet=mapping->get().to_backend.peek();
    if(!packet) return nullptr;
    input_owned=true;
    return env->NewDirectByteBuffer(const_cast<steve::OotToMcPacket*>(packet),sizeof(*packet));
}
JNIEXPORT void JNICALL Java_org_ocarina_bridge_NativeBridge_consume(JNIEnv* env,jclass) {
    if(!mapping || !input_owned) { fail(env,"no input slot owned"); return; }
    mapping->get().to_backend.consume(); input_owned=false;
}
JNIEXPORT jobject JNICALL Java_org_ocarina_bridge_NativeBridge_reserve(JNIEnv* env,jclass) {
    if(!mapping || output_owned) { fail(env,"output slot ownership violation"); return nullptr; }
    auto* packet=mapping->get().to_frontend.reserve();
    if(!packet) return nullptr;
    *packet={}; output_owned=true;
    packet->time_ns=steve::now_ns();
    return env->NewDirectByteBuffer(packet,sizeof(*packet));
}
JNIEXPORT void JNICALL Java_org_ocarina_bridge_NativeBridge_publish(JNIEnv* env,jclass) {
    if(!mapping || !output_owned) { fail(env,"no output slot owned"); return; }
    mapping->get().to_frontend.publish(); output_owned=false;
}
JNIEXPORT jlong JNICALL Java_org_ocarina_bridge_NativeBridge_now(JNIEnv*,jclass) { return static_cast<jlong>(steve::now_ns()); }
JNIEXPORT jboolean JNICALL Java_org_ocarina_bridge_NativeBridge_alive(JNIEnv*,jclass) {
    if(!mapping) return JNI_FALSE;
    auto& s=mapping->get();
    const auto heartbeat=s.heartbeat_ns.load(std::memory_order_acquire);
    const auto time=steve::now_ns();
    return s.ready.load(std::memory_order_acquire)==steve::magic &&
        (time<heartbeat || time-heartbeat<500'000'000);
}
JNIEXPORT void JNICALL Java_org_ocarina_bridge_NativeBridge_close(JNIEnv*,jclass) { input_owned=false; output_owned=false; mapping.reset(); }
}
