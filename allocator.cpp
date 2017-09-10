#include <features.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <bits/stdc++.h>

#include <cassert>

inline int get_offset(void * __ptr) {
	int offset = reinterpret_cast<long long>(__ptr) % getpagesize();
	if (offset == 0) {
		offset = getpagesize();
	}
	return offset;
}

inline size_t * get_start(void * __ptr) {
	int offset = get_offset(__ptr);
	void * alloc_ptr =
			reinterpret_cast<void *>(reinterpret_cast<long long>(__ptr) - offset);
	return reinterpret_cast<size_t *>(alloc_ptr);
}

extern "C" {

extern void *malloc(size_t __size) {
	void * nl = mmap(NULL, __size + sizeof(size_t),
	PROT_READ | PROT_WRITE | PROT_EXEC,
	MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	*reinterpret_cast<size_t *>(nl) = __size;
	return reinterpret_cast<size_t *>(nl) + 1;
}

extern void free(void *__ptr) {
	if (__ptr == nullptr) {
		return;
	}
	int offset = get_offset(__ptr);
	size_t * alloc_ptr = get_start(__ptr);
	size_t size = *alloc_ptr;
	munmap(alloc_ptr, size + offset);
}

extern void *calloc(size_t __nmemb, size_t __size) {
	void * ptr = malloc(__nmemb * __size);
	memset(ptr, 0, __nmemb * __size);
	return ptr;
}

extern void *realloc(void *__ptr, size_t __size) {
	void * newptr = malloc(__size);
	if (__ptr != nullptr) {
		size_t old_size = *get_start(__ptr);
		if (old_size > __size)
			old_size = __size;
		memcpy(newptr, __ptr, old_size);
		free(__ptr);
	}
	return newptr;
}

extern void cfree(void *__ptr) {
	free(__ptr);
}

extern void *memalign(size_t __alignment, size_t __size) {
	if (__alignment > (size_t) getpagesize()) {
		exit(11);
//		assert(false); // not yet implemented)))
//		return nullptr;
	} else if (__alignment > sizeof(size_t)) {
		void * new_alloc = mmap(NULL, __size + __alignment,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
		*reinterpret_cast<size_t *>(new_alloc) = __size;
		return reinterpret_cast<void *>(reinterpret_cast<long long>(new_alloc)
				+ __alignment);
	} else {
		return malloc(__size);
	}
}

extern void *valloc(size_t __size) {
	return memalign(getpagesize(), __size);
}

extern void *pvalloc(size_t __size) {
	return memalign(getpagesize(), __size);
}

}
