#include "list.h"
#include "list_sort.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef struct element {
	struct list_head list;
	char *val;
	int seq;
} element_t;

#define SAMPLES (1 << 20) + 1
#define STR_LEN 30

static void create_sample(struct list_head *head, int samples)
{
	for (int i = 0; i < samples; i++) {
		element_t *elem = malloc(sizeof(*elem));
		elem->val = malloc(STR_LEN);
		#if 1
		for (int j = 0; j < STR_LEN / 2; j++)
			elem->val[j] = 'a';
		for (int j = STR_LEN / 2; j < STR_LEN - 1; j++)
			elem->val[j] = rand();
		#else
		for (int j = 0; j < STR_LEN; j++)
			elem->val[j] = i;
		#endif
		elem->val[STR_LEN - 1] = '\0';
		elem->seq = i;
		list_add_tail(&elem->list, head);
	}
}

static void copy_list(struct list_head *from, struct list_head *to)
{
	if (list_empty(from))
		return;

	element_t *entry;
	list_for_each_entry(entry, from, list) {
		element_t *copy = malloc(sizeof(*copy));
		copy->val = strdup(entry->val);
		copy->seq = entry->seq;
		list_add_tail(&copy->list, to);
	}
}

static void free_list(struct list_head *head)
{
	element_t *entry, *safe;
	list_for_each_entry_safe(entry, safe, head, list) {
		list_del(&entry->list);
		free(entry->val);
		free(entry);
	}
}

int compare(void *priv, const struct list_head *a, const struct list_head *b)
{
	if (a == b)
		return 0;

	int res = strcmp(list_entry(a, element_t, list)->val,
			 list_entry(b, element_t, list)->val);

	if (priv) {
		*((int *)priv) += 1;
	}

	return res;
}

bool check_list(struct list_head *head, int count)
{
	if (list_empty(head))
		return count == 0;

	element_t *entry, *safe;
	list_for_each_entry_safe(entry, safe, head, list) {
		if (entry->list.next != head) {
			int cmp = strcmp(entry->val, safe->val);
			if (cmp > 0 || (cmp == 0 && entry->seq > safe->seq)) {
				return false;
			}
		}
	}
	return true;
}

typedef void (*test_func_t)(void *priv, struct list_head *head,
			    list_cmp_func_t cmp);

typedef struct test {
	test_func_t fp;
	char *name;
} test_t;

int main(void)
{
	struct list_head sample, warmdata, testdata;
	int count;
	int nums = SAMPLES;

	test_t tests[] = {
			   { list_sort, "list_sort" },
			   { shiverssort, "shiverssort" },
			   { list_sort, "list_sort" },
			   { shiverssort, "shiverssort" },
			   { list_sort, "list_sort" },
			   { shiverssort, "shiverssort" },
			   { list_sort, "list_sort" },
			   { timsort, "timsort" },
			   { NULL, NULL } },
	       *test = tests;

	INIT_LIST_HEAD(&sample);
	INIT_LIST_HEAD(&warmdata);
	INIT_LIST_HEAD(&testdata);

	srand((uintptr_t)&main ^ time(NULL));
	create_sample(&sample, nums);

	copy_list(&sample, &testdata);
	copy_list(&sample, &warmdata);
	free_list(&testdata);
	free_list(&warmdata);

	copy_list(&sample, &testdata);
	copy_list(&sample, &warmdata);
	free_list(&testdata);
	free_list(&warmdata);

	while (test->fp != NULL) {
		printf("==== Testing %s ====\n", test->name);
		/* Warm up */
		copy_list(&sample, &testdata);
		copy_list(&sample, &warmdata);
		test->fp(&count, &warmdata, compare);
		/* Test */
		clock_t begin;
		count = 0;
		begin = clock();
		test->fp(&count, &testdata, compare);
		printf("  Elapsed time:   %ld\n", clock() - begin);
		printf("  Comparisons:    %d\n", count);
		printf("  List is %s\n",
		       check_list(&testdata, nums) ? "sorted" : "not sorted");
		free_list(&testdata);
		free_list(&warmdata);
		test++;
	}

	return 0;
}
