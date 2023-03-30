#include "list.h"
#include "list_sort.h"

#include <stdint.h>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define MAX_MERGE_PENDING (sizeof(size_t) * 8) + 1

struct run {
	struct list_head *list;
	size_t len;
};

static struct list_head *merge(void *priv, list_cmp_func_t cmp,
			       struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head, **node;

	do {
		/* if equal, take 'a' -- important for sort stability */
		node = cmp(priv, a, b) <= 0 ? &a : &b;
		*tail = *node;
		tail = &(*node)->next;
	} while ((*node = (*node)->next));
	*tail = (struct list_head *)((uintptr_t)a | (uintptr_t)b);
	return head;
}

static void build_prev_link(struct list_head *head, struct list_head *tail,
			    struct list_head *list)
{
	tail->next = list;
	do {
		list->prev = tail;
		tail = list;
		list = list->next;
	} while (list);

	/* The final links to make a circular doubly-linked list */
	tail->next = head;
	head->prev = tail;
}

static void merge_final(void *priv, list_cmp_func_t cmp, struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head, **node;

	do {
		/* if equal, take 'a' -- important for sort stability */
		node = cmp(priv, a, b) <= 0 ? &a : &b;
		tail->next = *node;
		(*node)->prev = tail;
		tail = *node;
	} while ((*node = (*node)->next));
	b = (struct list_head *)((uintptr_t)a | (uintptr_t)b);

	/* Finish linking remainder of list b on to tail */
	build_prev_link(head, tail, b);
}

static struct list_head *find_run(void *priv, struct list_head *list,
				  size_t *len, list_cmp_func_t cmp)
{
	*len = 1;
	struct list_head *next = list->next;

	if (unlikely(next == NULL))
		return NULL;

	if (cmp(priv, list, next) > 0) {
		/* decending run, also reverse the list */
		struct list_head *prev = NULL;
		do {
			(*len)++;
			list->next = prev;
			prev = list;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) > 0);
		list->next = prev;
	} else {
		do {
			(*len)++;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) <= 0);
		list->next = NULL;
	}

	return next;
}

static void merge_at(void *priv, list_cmp_func_t cmp, struct run *at)
{
	at[0].list = merge(priv, cmp, at[0].list, at[1].list);
	at[0].len += at[1].len;
}

static struct run *merge_force_collapse(void *priv, list_cmp_func_t cmp,
					struct run *stk, struct run *tp)
{
	while ((tp - stk + 1) >= 3) {
		if (tp[-2].len < tp[0].len) {
			merge_at(priv, cmp, &tp[-2]);
			tp[-1] = tp[0];
		} else {
			merge_at(priv, cmp, &tp[-1]);
		}
		tp--;
	}
	return tp;
}

static inline bool log2_ge(size_t a, size_t b)
{
	return __builtin_clzl(a) <= __builtin_clzl(b);
}

static inline bool log2_g(size_t a, size_t b)
{
	return !log2_ge(b, a);
}

static struct run *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct run *stk, struct run *tp)
{
	int n;
	while ((n = tp - stk + 1) >= 3) {
		if (log2_g(tp[0].len, tp[-1].len) && tp[0].len < tp[-2].len) {
			merge_at(priv, cmp, &tp[-1]);
		} else if (log2_g(tp[0].len, tp[-2].len) ||
			   log2_g(tp[0].len, tp[-1].len) ||
			   (n > 3 && log2_ge(tp[-1].len, tp[-2].len))) {
			if (n == 4 && (log2_ge(tp[-1].len, tp[-3].len) ||
				       log2_g(tp[-2].len, tp[-3].len))) {
				merge_at(priv, cmp, &tp[-3]);
				tp[-2] = tp[-1];
				tp[-1] = tp[0];
			} else {
				merge_at(priv, cmp, &tp[-2]);
				tp[-1] = tp[0];
			}
		} else {
			break;
		}
		tp--;
	}

	return tp;
}

void shiverssort2(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *list = head->next;
	struct run stk[MAX_MERGE_PENDING], *tp = stk - 1;

	if (head == head->prev)
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		tp++;
		/* Find next run */
		tp->list = list;
		list = find_run(priv, list, &tp->len, cmp);
		tp = merge_collapse(priv, cmp, stk, tp);
	} while (list);

	/* End of input; merge together all the runs. */
	tp = merge_force_collapse(priv, cmp, stk, tp);

	/* The final merge; rebuild prev links */
	if (likely(tp > stk)) {
		merge_final(priv, cmp, head, stk[0].list, stk[1].list);
	} else {
		build_prev_link(head, head, stk->list);
	}
}
