#include "init.hpp"
#include "mpq/mpq_reader.hpp"

#include <jni.h>

namespace devilution {
namespace {

bool AreExtraFontsOutOfDateForMpqPath(const char *mpqPath)
{
	int32_t error = 0;
	std::optional<MpqArchive> archive = MpqArchive::Open(mpqPath, error);
	return error == 0 && archive && AreExtraFontsOutOfDate(*archive);
}

} // namespace
} // namespace devilution

extern "C" {
JNIEXPORT jboolean JNICALL Java_org_diasurgical_devilutionx_DevilutionXSDLActivity_areFontsOutOfDate(JNIEnv *env, jclass cls, jstring fonts_mpq)
{
	const char *mpqPath = env->GetStringUTFChars(fonts_mpq, nullptr);
	bool outOfDate = devilution::AreExtraFontsOutOfDateForMpqPath(mpqPath);
	env->ReleaseStringUTFChars(fonts_mpq, mpqPath);
	return outOfDate;
}
}
