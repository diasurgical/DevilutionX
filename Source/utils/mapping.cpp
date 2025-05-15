#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include <string>

#include "items.h"
#include "monster.h"
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "utils/ring.h"
#include "utils/shared.h"

namespace devilution {

#define ATTRS(align)								\
	__attribute__((section(".shared"),no_reorder))	\
	__attribute__ ((aligned(align)))

struct entry_ptr;
static struct entry_ptr *tail_entry;

struct entry_ptr {
	const char       *name;
	void             *addr;
	size_t           align;
	size_t           size;
	struct entry_ptr *next;

	entry_ptr(const char *name, void *addr, size_t align, size_t size):
		name(name), addr(addr), align(align),
		size(size), next(tail_entry) {
		tail_entry = this;
	}
};

#define DEFINE_VAR(aligned, type, named)		\
	ATTRS(aligned) __typeof__(type) named;		\
	static struct entry_ptr entry_##named(		\
		#named, &named, aligned, sizeof(named));

/*
 * Here we define all variables which should be shared
 */
namespace shared {
	DEFINE_VAR(2, uint16_t[2], maxdun);
	DEFINE_VAR(2, uint16_t[2], dmax);
	DEFINE_VAR(4, uint32_t, max_monsters);
	DEFINE_VAR(4, uint32_t, max_objects);
	DEFINE_VAR(4, uint32_t, max_tiles);
	DEFINE_VAR(4, uint32_t, max_items);
	DEFINE_VAR(4, uint32_t, num_mtypes);
	DEFINE_VAR(4, uint32_t, max_triggers);
	DEFINE_VAR(4, struct ring_queue, input_queue);
	DEFINE_VAR(4, struct ring_queue, events_queue);
	DEFINE_VAR(4, struct player_state, player);
	DEFINE_VAR(8, uint64_t, game_ticks);
	DEFINE_VAR(8, uint64_t, game_saves);
	DEFINE_VAR(8, uint64_t, game_loads);
}

/* monsters.cpp */
DEFINE_VAR(8, size_t, LevelMonsterTypeCount);
DEFINE_VAR(8, size_t, ActiveMonsterCount);
DEFINE_VAR(8, Monster[MaxMonsters], Monsters);
DEFINE_VAR(4, unsigned[MaxMonsters], ActiveMonsters);
DEFINE_VAR(4, int[NUM_MTYPES], MonsterKillCounts);

/* objects.cpp */
DEFINE_VAR(4, Object[MAXOBJECTS], Objects);
DEFINE_VAR(4, int[MAXOBJECTS], AvailableObjects);
DEFINE_VAR(4, int[MAXOBJECTS], ActiveObjects);
DEFINE_VAR(4, int, ActiveObjectCount);

/* diablo.cpp */
DEFINE_VAR(4, int, PauseMode);

/* items.cpp */
DEFINE_VAR(4, Item[MAXITEMS + 1], Items);
DEFINE_VAR(1, uint8_t[MAXITEMS], ActiveItems);
DEFINE_VAR(1, uint8_t, ActiveItemCount);
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dItem);

/* gendung.cpp */
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dTransVal);
DEFINE_VAR(1, DungeonFlag[MAXDUNX][MAXDUNY], dFlags);
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dPlayer);
DEFINE_VAR(2, int16_t[MAXDUNX][MAXDUNY], dMonster);
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dCorpse);
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dObject);
DEFINE_VAR(2, uint16_t[MAXDUNX][MAXDUNY], dPiece);
DEFINE_VAR(1, int8_t[MAXDUNX][MAXDUNY], dSpecial);
DEFINE_VAR(1, TileProperties[MAXTILES], SOLData);

/* trigs.cpp */
DEFINE_VAR(1, uint8_t, padding2);
DEFINE_VAR(4, int, numtrigs);
DEFINE_VAR(4, TriggerStruct[MAXTRIGGERS], trigs);

/*
 * End of sharing
 */

/* See the script.ld for the details */
extern "C" {
	int shared_section_start;
	int shared_section_end;
}

static void verify_no_padding(void)
{
	struct entry_ptr *ptr, *prev, *head_entry;

	/* Reverse chain */
	prev = NULL;
	ptr = tail_entry;
	do {
		struct entry_ptr *next;
		next = ptr->next;
		ptr->next = prev;
		prev = ptr;
		ptr = next;
	} while (ptr);
	head_entry = prev;

	/* Verify no padding */
	prev = NULL;
	for (ptr = head_entry; ptr; ptr = ptr->next) {
		if (!prev) {
			assert((char *)ptr->addr == (char *)&shared_section_start);
		} else {
			if ((char *)prev->addr + prev->size != (char *)ptr->addr) {
				printf("ALIGNMENT ERROR: %s (%p + %ld = %p) != %s (%p)\n",
					   prev->name, prev->addr, prev->size,
					   (char *)prev->addr + prev->size,
					   ptr->name, ptr->addr);
				exit(1);
			}
		}
		prev = ptr;
	}
	if ((char *)prev->addr + prev->size > (char *)&shared_section_end) {
		printf("BOUNDS ERROR: %s (%p + %ld = %p) > shared_section_end (%p)\n",
			   prev->name, prev->addr, prev->size,
			   (char *)prev->addr + prev->size,
			   &shared_section_end);
		exit(1);
	}
}

static void do_init_after_map(void)
{
	using namespace shared;

	maxdun[0] = MAXDUNX;
	maxdun[1] = MAXDUNY;
	dmax[0] = DMAXX;
	dmax[1] = DMAXY;
	max_monsters = MaxMonsters;
	max_objects  = MAXOBJECTS;
	max_tiles    = MAXTILES;
	max_items    = MAXITEMS;
	num_mtypes   = NUM_MTYPES;
	max_triggers = MAXTRIGGERS;
	game_ticks   = 0;
	game_saves   = 0;
	game_loads   = 0;

	ring_queue_init(&input_queue);
	ring_queue_init(&events_queue);
}

void shared::share_diablo_state(const std::string &mshared_path)
{
	char path[PATH_MAX];
	int fd, ret;
	void *addr;
	size_t len;

	verify_no_padding();

	len = (char *)&shared_section_end - (char *)&shared_section_start;

	snprintf(path, sizeof(path), "%s", mshared_path.c_str());
	fd = open(path, O_CREAT | O_RDWR,
			  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fd < 0) {
		perror("open: ");
		exit(1);
	}
	ret = ftruncate(fd, len);
	if (ret < 0) {
		perror("ftruncate: ");
		exit(1);
	}
	addr = mmap(&shared_section_start, len,
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED,
				fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		perror("mmap: ");
		exit(1);
	}

	do_init_after_map();
}

}
