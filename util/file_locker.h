#ifndef ZN_FILE_LOCKER_H
#define ZN_FILE_LOCKER_H

#include "errors.h"
#include "thread/mutex.h"
#include "thread/rw_lock.h"

#include <string>
#include <unordered_map>

namespace zylann {

// Performs software locking on paths,
// so that multiple threads (controlled by this module) wanting to access the same file will lock a shared mutex.
class FileLocker {
public:
	void lock_read(const std::string &fpath) {
		lock(fpath, true);
	}

	void lock_write(const std::string &fpath) {
		lock(fpath, false);
	}

	void unlock(const std::string &fpath) {
		unlock_internal(fpath);
	}

private:
	struct File {
		RWLock lock;
		bool read_only;
	};

	void lock(const std::string &fpath, bool read_only) {
		File *fp = nullptr;
		{
			MutexLock lock(_files_mutex);
			// Get or create.
			// Note, we never remove entries from the map
			fp = &_files[fpath];
		}

		if (read_only) {
			fp->lock.read_lock();
			// The read lock was acquired. It means nobody is writing.
			fp->read_only = true;

		} else {
			fp->lock.write_lock();
			// The write lock was acquired. It means only one thread is writing.
			fp->read_only = false;
		}
	}

	void unlock_internal(const std::string &fpath) {
		File *fp = nullptr;
		{
			MutexLock lock(_files_mutex);
			auto it = _files.find(fpath);
			if (it != _files.end()) {
				fp = &it->second;
			}
		}
		ZN_ASSERT_RETURN(fp != nullptr);
		// TODO FileAccess::reopen can have been called, nullifying my efforts to enforce thread sync :|
		// So for now please don't do that

		if (fp->read_only) {
			fp->lock.read_unlock();
		} else {
			fp->lock.write_unlock();
		}
	}

private:
	Mutex _files_mutex;
	std::unordered_map<std::string, File> _files;
};

} // namespace zylann

#endif // ZN_FILE_LOCKER_H
