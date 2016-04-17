#define __USE_MISC
#define _SYSTEM	1
#define _MINIX 1

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <minix/rs.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/rwsem.h>

#include <sys/types.h>
#include <errno.h>

static int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

int rwsemget(key_t key)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEMGET_KEY = key;

	r = _syscall(ipc_pt, IPC_RWSEMGET, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return m.RWSEMGET_RETID;
}

int rwsemdel(int semid)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEM_ID = semid;

	r = _syscall(ipc_pt, IPC_RWSEMDEL, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return 0;
}

int read_lock(int semid)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEM_ID = semid;

	r = _syscall(ipc_pt, IPC_READ_LOCK, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return 0;
}

int read_unlock(int semid)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEM_ID = semid;

	r = _syscall(ipc_pt, IPC_READ_UNLOCK, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return 0;
}

int write_lock(int semid)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEM_ID = semid;

	r = _syscall(ipc_pt, IPC_WRITE_LOCK, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return 0;
}

int write_unlock(int semid)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.RWSEM_ID = semid;

	r = _syscall(ipc_pt, IPC_WRITE_UNLOCK, &m);
	if (r != OK) {
		errno = r;
		return -1;
	}

	return 0;
}
