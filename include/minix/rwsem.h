#ifndef _RWSEM_H_
#define _RWSEM_H_

#include <sys/types.h>

#define MAX_RWSEM 500

int rwsemget(key_t key);
int rwsemdel(int semid);
int read_lock(int semid);
int read_unlock(int semid);
int write_lock(int semid);
int write_unlock(int semid);

#endif /* _RWSEM_H_ */
