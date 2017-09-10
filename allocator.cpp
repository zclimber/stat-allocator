#include <features.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <bits/stdc++.h>

#include <cassert>

extern "C" {

extern void *malloc(size_t __size) {
	void * nl = mmap(NULL, __size + sizeof(size_t),
	PROT_READ | PROT_WRITE | PROT_EXEC,
	MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	*reinterpret_cast<size_t *>(nl) = __size;
	return reinterpret_cast<size_t *>(nl) + 1;
}

extern void free(void *__ptr) {
	if(__ptr == nullptr){
		return;
	}
	int offset = reinterpret_cast<long long>(__ptr) % getpagesize();
	if (offset == 0) {
		offset = getpagesize();
	}
	void * alloc_ptr =
			reinterpret_cast<void *>(reinterpret_cast<long long>(__ptr) - offset);
	size_t size = *reinterpret_cast<size_t *>(alloc_ptr);
	munmap(alloc_ptr, size + offset);
}

extern void *calloc(size_t __nmemb, size_t __size) {
	void * ptr = malloc(__nmemb * __size);
	memset(ptr, 0, __nmemb * __size);
	return ptr;
}

extern void *realloc(void *__ptr, size_t __size) {
	void * newptr = malloc(__size);
	free(__ptr);
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
