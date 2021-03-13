#ifndef VOXEL_FILE_LOCKER_H
#define VOXEL_FILE_LOCKER_H

#include <core/hash_map.h>
#include <core/os/file_access.h>
#include <core/os/mutex.h>
#include <core/os/rw_lock.h>

// Performs software locking on paths,
// so that multiple threads (controlled by this module) wanting to access the same file will lock a shared mutex.
// Note: has nothing to do with voxels, it's just prefixed.
class VoxelFileLocker {
public:
	~VoxelFileLocker() {
		const String *key = nullptr;
		while ((key = _files.next(key))) {
			File *f = _files[*key];
			memdelete(f);
		}
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
		RWLock lock;
		bool read_only;
	};

	void lock(String fpath, bool read_only) {
		File *fp = nullptr;
		{
			MutexLock lock(_files_mutex);
			File **fpp = _files.getptr(fpath);

			if (fpp == nullptr) {
				fp = memnew(File);
				_files.set(fpath, fp);
			}
			else {
				fp = *fpp;
			}
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

	void unlock_internal(String fpath) {
		File *fp = nullptr;
		// I assume `get_path` returns the same string that was used to open it
		{
			MutexLock lock(_files_mutex);
			File **fpp = _files.getptr(fpath);
			if (fpp != nullptr) {
				fp = *fpp;
			}
		}
		ERR_FAIL_COND(fp == nullptr);
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
	// Had to use dynamic allocs because HashMap does not implement move semantics
	HashMap<String, File *> _files;
};

#endif // VOXEL_FILE_LOCKER_H
