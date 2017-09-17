#include "work_with_slabs.h"

#define PAGE_SIZE 4096

#define MAX_SLAB_ALLOC 64

constexpr long long SLAB_MAGIC = 0x7777777777770000;

int slab_t_size[] = { 8, 16, 24, 32, 48, 64 };
int select_slab[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
		2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 };

struct slab;

struct thread_slab_storage {

	std::atomic<slab *> slb[6];
	std::atomic_int full;
	std::atomic<bool> alive;
	void * alloc_in_storage(size_t size);
	void add_back_slab(int sslab, slab * s);

	thread_slab_storage();
	~thread_slab_storage();
};

struct slab {
	union {
		long long SLAB_MAGIC;
		struct {
			unsigned char elem_size;
			bool parent_alive;
		};
	};
	std::atomic<void *> freeptr;
	std::atomic_short freecnt;
	std::atomic<slab *> next;
	thread_slab_storage * parent;
	char data[PAGE_SIZE - 40];
	slab(unsigned char elem_size, thread_slab_storage * parent) {
		this->SLAB_MAGIC = 0x7777777777777777;
		this->elem_size = (unsigned char) elem_size;
		this->parent = parent;
		parent_alive = true;
		freeptr.store(reinterpret_cast<void *>(data));
		freecnt.store(short(sizeof(data) / elem_size));
		next = nullptr;
		for (int i = elem_size; i + elem_size <= PAGE_SIZE - 40; i +=
				elem_size) {
			*reinterpret_cast<void **>(data + i - elem_size) =
					reinterpret_cast<void *>(data + i);
		}
	}
	void * alloc_here() {
		void * res = freeptr.load();
		while (!freeptr.compare_exchange_weak(res,
				*reinterpret_cast<void **>(res))) {
		}
		freecnt.fetch_sub(1);
		return res;
	}
	void free_here(void * ptr) {
		void * oldfree = freeptr.load();
		*reinterpret_cast<void **>(ptr) = oldfree;
		while (!freeptr.compare_exchange_weak(oldfree, ptr)) {
			*reinterpret_cast<void **>(ptr) = oldfree;
		}
//		*reinterpret_cast<void **>(ptr) = freeptr.exchange(ptr);
		int fcnt = freecnt.fetch_add(1);
		if (fcnt == 0 && parent_alive) {
			if (parent->alive.load()) {
				parent->add_back_slab(select_slab[elem_size], this);
			} else {
				parent_alive = false;
			}
		}
	}
};

struct slab_free_pages {
	static const int FREE_PAGES_MAX = 200;
	std::mutex m;
	int free_pages_count;
	void * pages[FREE_PAGES_MAX];
	slab * get_slab(int elem_size, thread_slab_storage * parent) {
//		m.native_handle();
		std::lock_guard<std::mutex> lok(m);
		if (free_pages_count == 0) {
#ifdef _WIN64
			pages[free_pages_count++] = malloc(sizeof(slab));
#else
			pages[free_pages_count++] = mmap(nullptr, sizeof(slab),
			PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
		}
		return new (pages[--free_pages_count]) slab(elem_size, parent);
	}
	void recycle_slab(slab * slab) {
		slab->~slab();
		std::lock_guard<std::mutex> lok(m);
		if (free_pages_count < FREE_PAGES_MAX) {
			pages[free_pages_count++] = slab;
		} else {
#ifdef _WIN64
			free(slab);
#else
			munmap(slab, sizeof(slab));
#endif
		}
	}
};

slab_free_pages pages;

thread_slab_storage::thread_slab_storage() {
	full.store(0);
	for (int i = 0; i < 6; i++) {
		slb[i].store(nullptr);
	}
	alive.store(false);
}

thread_slab_storage::~thread_slab_storage() {
	assert(!alive.load());
}

void * thread_slab_storage::alloc_in_storage(size_t size) {
	int sslab = select_slab[size];
	slab * s = slb[sslab].load();
	if (s == nullptr) {
		s = pages.get_slab(slab_t_size[sslab], this);
		full.fetch_add(1);
	} else {
		while (!slb[sslab].compare_exchange_weak(s, s->next.load())) {
		}
	}
	void * res = s->alloc_here();
	if (s->freecnt.load() != 0) {
		add_back_slab(sslab, s);
	}
	return res;
}

void thread_slab_storage::add_back_slab(int sslab, slab * s) {
	slab * top;
	s->next = top = slb[sslab].load();
	while (!slb[sslab].compare_exchange_weak(top, s)) {
		s->next = top;
	}
}

thread_slab_storage storages[200];

std::atomic_int cr_storage;

struct thread_slab_ptr {
	thread_slab_storage * storage;
	thread_slab_ptr() {
		storage = nullptr;
		while (storage == nullptr) {
			int id = cr_storage.fetch_add(1) % 200;
			if (!storages[id].alive.load() && storages[id].full.load() == 0) {
				storages[id].alive.store(true);
				storage = storages + id;
			}
		}
	}
	thread_slab_storage * operator()() {
		return storage;
	}
	~thread_slab_ptr() {
		storage->alive.store(false);
		for (int i = 0; i < 6; i++) {
			slab * s = nullptr;
			while ((s = storage->slb[i].exchange(nullptr)) != nullptr) {
				do {
					s->parent_alive = false;
					s = s->next.load();
				} while (s != nullptr);
			}
		}
	}
}
;

thread_local thread_slab_ptr my_storage;

void init_slab_allocation() {
	static_assert(sizeof(select_slab) == (MAX_SLAB_ALLOC + 1) * sizeof(int), "SLAB resolving array has wrong size");
	static_assert(sizeof(slab) == PAGE_SIZE, "SLAB block size not equal to page size");
}

void *alloc_block_in_slab(size_t size) {
	if (size > MAX_SLAB_ALLOC) {
		return nullptr;
	}
	return my_storage()->alloc_in_storage(size);
}

constexpr long long PAGE_MASK = ~((long long) PAGE_SIZE - 1);

void free_block_in_slab(void *ptr) {
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	if ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC) {
		slb->free_here(ptr);
	}
}

void *realloc_block_in_slab(void *ptr, size_t new_size) {
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	if ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC) {
		if (new_size <= slb->elem_size) {
			return ptr;
		} else {
			void * new_ptr = malloc(new_size);
			memcpy(new_ptr, ptr, slb->elem_size);
			free_block_in_slab(ptr);
			return new_ptr;
		}
	} else {
		return nullptr;
	}
}

bool is_allocated_by_slab(void* ptr) {
	if (ptr == nullptr) {
		return false;
	}
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	return ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC);
}
