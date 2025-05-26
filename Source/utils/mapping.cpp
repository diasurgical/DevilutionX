#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include <string>

#include "utils/ring.h"
#include "utils/shared.h"

extern char __bss_start;
extern char _end;

namespace devilution {

/*
 * Here we define all variables which should be shared
 */
namespace shared {
	struct ring_queue input_queue;
	struct ring_queue events_queue;
	struct Player     player;
	uint64_t game_ticks;
	uint64_t game_saves;
	uint64_t game_loads;
}

/**
 * share_diablo_state() - finds all mapped regions where '__bss_start'
 * and '_end' lie, expecting these regions to have 'rw-p' permissions
 * and an executable binary as a mapped file. These mapped regions
 * will be remapped to @mshared_path, so that an external application
 * can access the '.data' and '.bss' sections of this application.
 */
void shared::share_diablo_state(const std::string &mshared_path)
{
#ifdef HAVE_LINKER_BSS_SYMBOLS
	// This is the simplest way to determine the region boundaries of
	// the .data and .bss sections without parsing the ELF itself. It
	// should work for most architectures where the binary is built
	// using GNU tools.
	uintptr_t region_start = (uintptr_t)&__bss_start;
	uintptr_t region_end = (uintptr_t)&_end;

	char exe_path[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	exe_path[len] = '\0';

	FILE *fp = fopen("/proc/self/maps", "r");
	if (!fp) {
		perror("fopen");
		exit(1);
	}

	uintptr_t start = 0, end = 0, offset = 0;
	char line[4096];
	int found = 0;

	while (fgets(line, sizeof(line), fp)) {
		uintptr_t s, e, o;
		char perms[5];
		char path[256];
		int n;

		path[0] = '\0';
		perms[0] = '\0';

		n = sscanf(line, "%lx-%lx %4s %lx %*s %*s %[^\n]", &s, &e, perms, &o, path);
		if (n < 4) {
			// Wow, can't parse maps?
			fprintf(stderr, "Can't parse maps\n");
			fclose(fp);
			exit(1);
		}
		if (!found && s <= region_start && region_start < e) {
			if (strcmp(perms, "rw-p") || strcmp(path, exe_path)) {
				// Expect 'rw' permissions and mapping of an exe binary
				fprintf(stderr, "Can't find correct mapped region: incorrect permissions or mapping path\n");
				fclose(fp);
				exit(1);
			}
			start = s;
			end = e;
			offset = o;
			if (s < region_end && region_end <= e) {
				// One mapped region fits everything
				found = 2;
				break;
			}
			found = 1;
		} else if (found) {
			if (strcmp(perms, "rw-p")) {
				// Expect 'rw' permissions and mapping of an exe binary
				fprintf(stderr, "Can't find correct mapped region: incorrect permissions\n");
				fclose(fp);
				exit(1);
			}
			if (end != s) {
				// Expect contiguous regions
				fprintf(stderr, "%lx %lx\n", s, end);
				fprintf(stderr, "Can't find correct mapped region: regions must be contiguous\n");
				fclose(fp);
				exit(1);
			}
			end = e;
			if (s < region_end && region_end <= e) {
				found = 2;
				break;
			}
		}
	}
	fclose(fp);

	if (!start || !end || found != 2) {
		fprintf(stderr, "Could not find suitable contiguous mappings\n");
		exit(1);
	}

	int fd = open(mshared_path.c_str(), O_CREAT | O_RDWR,
				  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	size_t length = end - start;
	if (ftruncate(fd, length) < 0) {
		perror("ftruncate");
		close(fd);
		exit(1);
	}

	// Write current memory content to the file
	if (write(fd, (void *)start, length) != (ssize_t)length) {
		perror("write");
		close(fd);
		exit(1);
	}

	// Remap the entire region as shared with 'rw' permissions
	void *new_map = mmap((void *)start, length, PROT_READ | PROT_WRITE,
						 MAP_FIXED | MAP_SHARED, fd, 0);
	if (new_map == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	printf("Remapped region %lx-%lx (offset %lx, length %lx) as MAP_SHARED\n",
		   start, end, offset, length);
	close(fd);
#else // HAVE_LINKER_BSS_SYMBOLS
	fprintf(stderr, "Sharing of internal state is unsupported due to the absence of linker BSS symbols.\n");
	exit(1);
#endif
}

}
