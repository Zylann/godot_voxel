#ifndef VOXEL_FILE_LOCKER_H
#define VOXEL_FILE_LOCKER_H

#include <core/hash_map.h>
#include <core/os/file_access.h>
#include <core/os/rw_lock.h>

// Performs software locking on paths,
// so that multiple threads (controlled by this module) wanting to access the same file will lock a shared mutex.
// Note: has nothing to do with voxels, it's just prefixed.
class VoxelFileLocker {
public:
	VoxelFileLocker() {
		_files_mutex = Mutex::create();
	}

	~VoxelFileLocker() {
		{
			MutexLock lock(_files_mutex);
			const String *key = nullptr;
			while ((key = _files.next(key))) {
				File *fp = _files.getptr(*key);
				memdelete(fp->lock);
			}
		}
		memdelete(_files_mutex);
	}

	void lock_read(String fpath) {
		lock(fpath, true);
	}

	void lock_write(String fpath) {
		lock(fpath, false);
	}

	void unlock(String fpath) {
		unlock_internal(fpath);
	}

private:
	struct File {
		RWLock *lock;
		bool read_only;
	};

	void lock(String fpath, bool read_only) {
		File *fp = nullptr;
		{
			MutexLock lock(_files_mutex);
			fp = _files.getptr(fpath);

			if (fp == nullptr) {
				File f;
				f.lock = RWLock::create();
				_files.set(fpath, f);
				fp = _files.getptr(fpath);
			}
		}

		if (read_only) {
			fp->lock->read_lock();
			// The read lock was acquired. It means nobody is writing.
			fp->read_only = true;

		} else {
			fp->lock->write_lock();
			// The write lock was acquired. It means only one thread is writing.
			fp->read_only = false;
		}
	}

	void unlock_internal(String fpath) {
		File *fp = nullptr;
		// I assume `get_path` returns the same string that was used to open it
		{
			MutexLock lock(_files_mutex);
			fp = _files.getptr(fpath);
		}
		ERR_FAIL_COND(fp == nullptr);
		// TODO FileAccess::reopen can have been called, nullifying my efforts to enforce thread sync :|
		// So for now please don't do that

		if (fp->read_only) {
			fp->lock->read_unlock();
		} else {
			fp->lock->write_unlock();
		}
	}

private:
	Mutex *_files_mutex;
	HashMap<String, File> _files;
};

#endif // VOXEL_FILE_LOCKER_H
