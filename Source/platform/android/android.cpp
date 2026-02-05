#include "init.hpp"
#include "mpq/mpq_reader.hpp"

#include <jni.h>
#include <pthread.h>

namespace devilution {

// Global Java VM pointer - set during JNI initialization
static JavaVM *g_jvm = nullptr;
static jobject g_activity = nullptr;

// JNI method cache for accessibility functions
struct AndroidJNIMethods {
	jmethodID isScreenReaderEnabled;
	jmethodID accessibilitySpeak;
	bool initialized;
} g_jniMethods = { nullptr, nullptr, false };

// Thread-local storage for JNIEnv
static pthread_key_t g_jniEnvKey;

// Initialize JNI environment key
static void JNIKeyDestructor(void *env)
{
	// Don't detach - let SDL handle it
}

static void InitializeJNIKey()
{
	static bool initialized = false;
	if (initialized)
		return;

	pthread_key_create(&g_jniEnvKey, JNIKeyDestructor);
	initialized = true;
}

// Get JNI environment for current thread
static JNIEnv* GetJNI()
{
	InitializeJNIKey();

	JNIEnv *env = (JNIEnv *)pthread_getspecific(g_jniEnvKey);
	if (env)
		return env;

	if (g_jvm == nullptr)
		return nullptr;

	// Get or attach the current thread
	int status = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (status == JNI_EDETACHED) {
		// Thread not attached, attach it
		status = g_jvm->AttachCurrentThread(&env, nullptr);
		if (status < 0)
			return nullptr;
		pthread_setspecific(g_jniEnvKey, env);
	} else if (status != JNI_OK) {
		return nullptr;
	}

	return env;
}

static bool AreExtraFontsOutOfDateForMpqPath(const char *mpqPath)
{
	int32_t error = 0;
	std::optional<MpqArchive> archive = MpqArchive::Open(mpqPath, error);
	return error == 0 && archive && AreExtraFontsOutOfDate(*archive);
}

// Initialize JNI method IDs for accessibility
// This should be called once during initialization
static void InitializeAccessibilityJNI(JNIEnv *env)
{
	if (g_jniMethods.initialized)
		return;

	// Get the DevilutionXSDLActivity class
	jclass activityClass = env->FindClass("org/diasurgical/devilutionx/DevilutionXSDLActivity");
	if (activityClass == nullptr) {
		return;
	}

	// Cache method IDs for accessibility functions
	g_jniMethods.isScreenReaderEnabled = env->GetMethodID(activityClass, "isScreenReaderEnabled", "()Z");
	g_jniMethods.accessibilitySpeak = env->GetMethodID(activityClass, "accessibilitySpeak", "(Ljava/lang/String;)V");

	if (g_jniMethods.isScreenReaderEnabled && g_jniMethods.accessibilitySpeak) {
		g_jniMethods.initialized = true;
	}

	env->DeleteLocalRef(activityClass);
}

// Public accessibility functions for Android
namespace accessibility {

bool InitializeScreenReaderAndroid()
{
	// JNI is initialized when nativeInitAccessibility is called from Java
	// This function is kept for compatibility but the actual initialization
	// happens in Java_org_diasurgical_devilutionx_DevilutionXSDLActivity_nativeInitAccessibility
	return g_jniMethods.initialized;
}

void ShutDownScreenReaderAndroid()
{
	// Clean up the activity reference
	if (g_activity != nullptr) {
		JNIEnv *env = GetJNI();
		if (env != nullptr) {
			env->DeleteGlobalRef(g_activity);
		}
		g_activity = nullptr;
	}

	g_jniMethods.initialized = false;
	g_jniMethods.isScreenReaderEnabled = nullptr;
	g_jniMethods.accessibilitySpeak = nullptr;
}

void SpeakTextAndroid(const char *text)
{
	if (!g_jniMethods.initialized)
		return;

	JNIEnv *env = GetJNI();
	if (env == nullptr || g_activity == nullptr)
		return;

	// Create a Java string from the text
	jstring jText = env->NewStringUTF(text);
	if (jText == nullptr)
		return;

	// Call the accessibilitySpeak method
	env->CallVoidMethod(g_activity, g_jniMethods.accessibilitySpeak, jText);

	// Clean up
	env->DeleteLocalRef(jText);
}

bool IsScreenReaderEnabledAndroid()
{
	if (!g_jniMethods.initialized)
		return false;

	JNIEnv *env = GetJNI();
	if (env == nullptr || g_activity == nullptr)
		return false;

	// Call the isScreenReaderEnabled method
	jboolean result = env->CallBooleanMethod(g_activity, g_jniMethods.isScreenReaderEnabled);

	return result == JNI_TRUE;
}

} // namespace accessibility
} // namespace devilution

// JNI initialization function called from Java during Activity initialization
extern "C" {
JNIEXPORT void JNICALL Java_org_diasurgical_devilutionx_DevilutionXSDLActivity_nativeInitAccessibility(
	JNIEnv *env, jobject thiz)
{
	// Store the Java VM pointer
	if (devilution::g_jvm == nullptr) {
		env->GetJavaVM(&devilution::g_jvm);
	}

	// Store a global reference to the activity
	if (devilution::g_activity != nullptr) {
		env->DeleteGlobalRef(devilution::g_activity);
	}
	devilution::g_activity = env->NewGlobalRef(thiz);

	// Initialize the JNI method cache
	devilution::InitializeAccessibilityJNI(env);
}

JNIEXPORT jboolean JNICALL Java_org_diasurgical_devilutionx_DevilutionXSDLActivity_areFontsOutOfDate(JNIEnv *env, jclass cls, jstring fonts_mpq)
{
	const char *mpqPath = env->GetStringUTFChars(fonts_mpq, nullptr);
	bool outOfDate = devilution::AreExtraFontsOutOfDateForMpqPath(mpqPath);
	env->ReleaseStringUTFChars(fonts_mpq, mpqPath);
	return outOfDate;
}
}
