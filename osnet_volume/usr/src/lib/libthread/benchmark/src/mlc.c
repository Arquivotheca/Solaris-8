#include <thread.h>

mutex_enter(mp)
	mutex_t *mp;
{
	mutex_lock(mp);
}

mutex_exit(mp)
	mutex_t *mp;
{
	mutex_unlock(mp);
}

_lmutex_enter(mp)
	mutex_t *mp;
{
	_lmutex_lock(mp);
}

_lmutex_exit(mp)
	mutex_t *mp;
{
	_lmutex_unlock(mp);
}

thread_t
thread_get_id()
{
	return (thr_self());
}
