#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

int sem_unlink(const char *name)
{
	return shm_unlink(name);
}
