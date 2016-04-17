#include "inc.h"

#include <assert.h>
#include <minix/rwsem.h>

/* Linked list of waiting clients. */

struct rwsem_queue_item {
	endpoint_t who;
	struct rwsem_queue_item *next;
};

struct rwsem_queue {
	struct rwsem_queue_item *first;
	struct rwsem_queue_item *last;
};

static void rwsem_queue_init(struct rwsem_queue *queue)
{
	queue->first = NULL;
	queue->last = NULL;
}

static int rwsem_queue_push(struct rwsem_queue *queue, endpoint_t who)
{
	struct rwsem_queue_item *item;

	item = malloc(sizeof(struct rwsem_queue_item));
	if (!item)
		return 0;

	item->who = who;
	item->next = NULL;

	if (queue->last == NULL) {
		queue->first = item;
		queue->last = item;
	} else {
		queue->last->next = item;
		queue->last = item;
	}

	return 1;
}

static endpoint_t rwsem_queue_pop(struct rwsem_queue *queue)
{
	struct rwsem_queue_item *item;
	endpoint_t who;

	item = queue->first;
	assert(item);
	who = item->who;

	queue->first = item->next;
	if (queue->last == item)
		queue->last = NULL;
	free(item);

	return who;
}

static int rwsem_queue_empty(struct rwsem_queue *queue)
{
	return queue->first == NULL;
}

/* RW semaphore structure. */

enum rwsem_state {
	RWSEM_FREE   = 0,
	RWSEM_ACTIVE = 1,
	RWSEM_CLOSED = 2,
};

struct rwsem {
	enum rwsem_state state;
	key_t key;
	int id;

	int readers_in;
	int writers_in;
	struct rwsem_queue readers_waiting;
	struct rwsem_queue writers_waiting;
};

static struct rwsem rwsem_list[MAX_RWSEM];
static int rwsem_count = 0;
static int rwsem_next = 0;

/* Finds an active/closed rwsem by a key. */
static struct rwsem *rwsem_find(key_t key)
{
	int i;
	for (i = 0; i < MAX_RWSEM; ++i)
		if (rwsem_list[i].state != RWSEM_FREE && rwsem_list[i].key == key)
			return &rwsem_list[i];
	return NULL;
}

/* Checks if an active/closed rwsem has no clients inside. */
static int rwsem_empty(struct rwsem *rwsem)
{
	assert(rwsem->state != RWSEM_FREE);
	return rwsem->readers_in == 0 && rwsem->writers_in == 0;
}

/* Deletes an empty, closed rwsem. */
static void rwsem_delete(struct rwsem *rwsem)
{
	message m;
	endpoint_t who;

	assert(rwsem->state == RWSEM_CLOSED);
	assert(rwsem_empty(rwsem));

	/* Wake the processes with EINTR error. */
	memset(&m, 0, sizeof(message));
	m.m_type = EINTR;
	while (!rwsem_queue_empty(&rwsem->readers_waiting)) {
		who = rwsem_queue_pop(&rwsem->readers_waiting);
		sendnb(who, &m);
	}
	while (!rwsem_queue_empty(&rwsem->writers_waiting)) {
		who = rwsem_queue_pop(&rwsem->writers_waiting);
		sendnb(who, &m);
	}

	rwsem->state = RWSEM_FREE;
}

/* Checks if an id points to a valid active/closed semaphore. */
static int rwsem_check_id(int id)
{
	if (id < 0 || id >= MAX_RWSEM)
		return 0;

	if (rwsem_list[id].state == RWSEM_FREE)
		return 0;

	return 1;
}

/* Send an empty message. */
static void rwsem_send(int r, endpoint_t who)
{
	message m;
	memset(&m, 0, sizeof(message));
	m.m_type = r;
	sendnb(who, &m);
}

int do_rwsemget(message *m)
{
	key_t key;
	int id;
	struct rwsem *rwsem;

	key = m->RWSEMGET_KEY;

	if ((rwsem = rwsem_find(key))) {
		id = rwsem->id;
	} else {
		if (rwsem_count == MAX_RWSEM)
			return EAGAIN;

		/* Find a free id. */
		while (rwsem_list[rwsem_next].state != RWSEM_FREE)
			rwsem_next = (rwsem_next + 1) % MAX_RWSEM;

		/* Create a new semaphore. */
		rwsem = &rwsem_list[rwsem_next];
		rwsem->state = RWSEM_ACTIVE;
		rwsem->key = key;
		rwsem->id = id = rwsem_next;
		rwsem->readers_in = 0;
		rwsem->writers_in = 0;
		rwsem_queue_init(&rwsem->readers_waiting);
		rwsem_queue_init(&rwsem->writers_waiting);

		rwsem_count += 1;
		rwsem_next = (rwsem_next + 1) % MAX_RWSEM;
	}

	m->RWSEMGET_RETID = id;
	return OK;
}

int do_rwsemdel(message *m)
{
	int id;
	struct rwsem *rwsem;

	id = m->RWSEM_ID;
	if (!rwsem_check_id(id))
		return ENOENT;

	rwsem = &rwsem_list[id];
	if (rwsem->state == RWSEM_CLOSED)
		return EINTR;

	/* Delete the semaphore right now if it is empty, otherwise
	   it will be deleted after the last client leaves. */
	rwsem->state = RWSEM_CLOSED;
	if (rwsem->readers_in == 0 && rwsem->writers_in == 0) {
		rwsem_delete(rwsem);
	}

	return OK;
}

int do_read_lock(message *m)
{
	int id;
	struct rwsem *rwsem;

	id = m->RWSEM_ID;
	if (!rwsem_check_id(id)) {
		rwsem_send(ENOENT, who_e);
		return 0;
	}

	rwsem = &rwsem_list[id];
	if (rwsem->state == RWSEM_CLOSED) {
		rwsem_send(EINTR, who_e);
		return 0;
	}

	/* Go in if no writers are in or waiting, otherwise queue. */
	if (rwsem->writers_in == 0 && rwsem_queue_empty(&rwsem->writers_waiting)) {
		rwsem->readers_in += 1;
		rwsem_send(OK, who_e);
	} else if (!rwsem_queue_push(&rwsem->readers_waiting, who_e)) {
		rwsem_send(ENOMEM, who_e);
	}

	return 0;
}

int do_read_unlock(message *m)
{
	int id;
	struct rwsem *rwsem;

	id = m->RWSEM_ID;
	if (!rwsem_check_id(id))
		return ENOENT;

	rwsem = &rwsem_list[id];
	if (rwsem->readers_in == 0)
		return EPERM;
	rwsem->readers_in -= 1;

	if (rwsem->readers_in == 0) {
		if (rwsem->state == RWSEM_CLOSED) {
			/* The last client deletes the semaphore. */
			rwsem_delete(rwsem);
		} else if (!rwsem_queue_empty(&rwsem->writers_waiting)) {
			/* The last reader wakes up a writer, if possible.*/
			rwsem->writers_in += 1;
			rwsem_send(OK, rwsem_queue_pop(&rwsem->writers_waiting));
		}
	}

	return OK;
}

int do_write_lock(message *m)
{
	int id;
	struct rwsem *rwsem;

	id = m->RWSEM_ID;
	if (!rwsem_check_id(id)) {
		rwsem_send(ENOENT, who_e);
		return 0;
	}

	rwsem = &rwsem_list[id];
	if (rwsem->state == RWSEM_CLOSED) {
		rwsem_send(EINTR, who_e);
		return 0;
	}

	/* Go in if empty, otherwise queue. */
	if (rwsem_empty(rwsem)) {
		rwsem->writers_in += 1;
		rwsem_send(OK, who_e);
	} else if (!rwsem_queue_push(&rwsem->writers_waiting, who_e)) {
		rwsem_send(ENOMEM, who_e);
	}

	return 0;
}

int do_write_unlock(message *m)
{
	int id;
	struct rwsem *rwsem;

	id = m->RWSEM_ID;
	if (!rwsem_check_id(id))
		return ENOENT;

	rwsem = &rwsem_list[id];
	if (rwsem->writers_in == 0)
		return EPERM;
	rwsem->writers_in -= 1;
	assert(rwsem->writers_in == 0);

	if (rwsem->state == RWSEM_CLOSED) {
		/* The last client deletes the semaphore. */
		rwsem_delete(rwsem);
	} else if (!rwsem_queue_empty(&rwsem->readers_waiting)) {
		/* Let in the readers. */
		while (!rwsem_queue_empty(&rwsem->readers_waiting)) {
			rwsem->readers_in += 1;
			rwsem_send(OK, rwsem_queue_pop(&rwsem->readers_waiting));
		}
	} else if (!rwsem_queue_empty(&rwsem->writers_waiting)) {
		/* Let in a writer. */
		rwsem->writers_in += 1;
		rwsem_send(OK, rwsem_queue_pop(&rwsem->writers_waiting));
	}

	return OK;
}
