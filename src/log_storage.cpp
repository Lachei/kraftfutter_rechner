#include "log_storage.h"

log_storage& log_storage::Default() {
	static log_storage storage{};
	return storage;
}

