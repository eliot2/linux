/**
 * HW4 Unit Tests.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* your code must define these functions */
extern void my_malloc_stats(void);
extern void *my_malloc(size_t size);
extern void my_free(void *ptr);
extern void *my_realloc(void *ptr, size_t size);
extern void my_empty_pages();

static unsigned int num_caught_faults;

static void fault_handler(int signum, siginfo_t *siginfo __attribute__((unused)), void *context __attribute__((unused))) {
	printf("Caught signal %d: %s!\n", signum, strsignal(signum));
	num_caught_faults++;
}

#define CHECK_IS_NOT_NULL(ptrA) \
	do { \
		if ((ptrA) != NULL) { \
			test_passed++; \
		} else { \
			test_failed++; \
			printf("%d: FAIL\n", __LINE__); \
		} \
	} while(0);

#define CHECK_IS_EQUAL(valA,valB) \
	do { \
		if ((valA) == (valB)) { \
			test_passed++; \
		} else { \
			test_failed++; \
			printf("%d: FAIL\n", __LINE__); \
		} \
	} while(0);

void hw4_test(void) {
	unsigned test_passed = 0;
	unsigned test_failed = 0;

	printf("Test 1: Display initialized memory\n");
	my_malloc_stats();
	test_passed++;

	printf("Test 2: Simple allocations\n");
	void *m1, *m2;
	m1 = my_malloc(30);
	CHECK_IS_NOT_NULL(m1);
	memset(m1, 'A', 30);
	m2 = my_malloc(50);
    CHECK_IS_NOT_NULL(m2);
	memset(m2, 'B', 33);

	printf("Test 3: Simple freeing\n");
	my_free(m1);
	void *m3 = my_malloc(15);
	CHECK_IS_EQUAL(m3, m1);
	memset(m3, 'C', 8);
	my_free(m3);
    my_malloc_stats();
    my_empty_pages();

	printf("Test 4: Out of memory condition\n");
	errno = 0;
	m1 = my_malloc(449);
	CHECK_IS_EQUAL(m1, NULL);
	CHECK_IS_EQUAL(errno, ENOMEM);
    my_malloc_stats();
    my_empty_pages();

	printf("Test 5: Double-free\n");
	struct sigaction sa = {
		.sa_sigaction = fault_handler,
		.sa_flags = SA_SIGINFO,
	};
	sigemptyset(&sa.sa_mask);
	struct sigaction orig_sa;
	if (sigaction(SIGSEGV, &sa, &orig_sa) < 0) {
		exit(EXIT_FAILURE);
	}
	my_free(m2);
	my_free(m2);
	CHECK_IS_EQUAL(num_caught_faults, 1);
	if (sigaction(SIGSEGV, &orig_sa, NULL) < 0) {
		exit(EXIT_FAILURE);
	}
	my_malloc_stats();
    my_empty_pages();

	printf("Test 6: Increasing memory\n");
	m1 = my_realloc(NULL, 129);
	CHECK_IS_NOT_NULL(m1);
	memset(m1, 'D', 129);
    my_empty_pages();
    my_malloc_stats();
    m2 = my_realloc(m1, 192);
	CHECK_IS_NOT_NULL(m2);
    my_malloc_stats();
    my_empty_pages();

	printf("Test 7: Decreasing memory\n");
	m3 = my_realloc(m2, 64);
    CHECK_IS_EQUAL(m2, m3);
    m1 = my_malloc(256);
	CHECK_IS_NOT_NULL(m1);
	memset(m1, 'E', 256);
    my_malloc_stats();
    my_empty_pages();

	printf("%u tests passed, %u tests failed.\n", test_passed, test_failed);
}
