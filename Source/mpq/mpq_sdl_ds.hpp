#pragma once

#include <mpqfs/mpqfs.h>

namespace devilution {

void *OpenAsMpqfsDataSource(const char *path, const char *mode, mpqfs_io_t &io);

} // namespace devilution
