#include "fileio.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <thread>
#include <unordered_map>
#include "str_util.h"

#define  READALL_OK          0  /* Success */
#define  READALL_INVALID    -1  /* Invalid parameters */
#define  READALL_ERROR      -2  /* Stream error */
#define  READALL_TOOMUCH    -3  /* Too much input */
#define  READALL_NOMEM      -4  /* Out of memory */
/* Size of each input chunk to be
   read and allocate for. */
#ifndef  READALL_CHUNK
#define  READALL_CHUNK  (1<<21) /* 2MB */
#endif


/* This function returns one of the READALL_ constants above.
   If the return value is zero == READALL_OK, then:
     (*dataptr) points to a dynamically allocated buffer, with
     (*sizeptr) chars read from the file.
     The buffer is allocated for one extra char, which is NUL,
     and automatically appended after the data.
   Initial values of (*dataptr) and (*sizeptr) are ignored.
*/
int readall(FILE *in, char **dataptr, size_t *sizeptr)
{
    char  *data = NULL, *temp;
    size_t size = 0;
    size_t used = 0;
    size_t n;

    /* None of the parameters can be NULL. */
    if (in == NULL || dataptr == NULL || sizeptr == NULL)
        return READALL_INVALID;

    /* A read error already occurred? */
    if (ferror(in))
        return READALL_ERROR;

    while (1) {

        if (used + READALL_CHUNK + 1 > size) {
            size = used + READALL_CHUNK + 1;

            /* Overflow check. Some ANSI C compilers
               may optimize this away, though. */
            if (size <= used) {
                free(data);
                return READALL_TOOMUCH;
            }

            temp = (char*)realloc(data, size);
            if (temp == NULL) {
                free(data);
                return READALL_NOMEM;
            }
            data = temp;
        }

        n = fread(data + used, 1, READALL_CHUNK, in);
        if (n == 0)
            break;

        used += n;
    }

    if (ferror(in)) {
        free(data);
        return READALL_ERROR;
    }

    temp = (char*)realloc(data, used + 1);
    if (temp == NULL) {
        free(data);
        return READALL_NOMEM;
    }
    data = temp;
    data[used] = '\0';

    *dataptr = data;
    *sizeptr = used;

    return READALL_OK;
}
int64_t ReadFileText(const String& filename, String& out) {
	const char* fname = StringAsCStr(filename);
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		int err = errno;
		printf("Failed opening file %s: %s (%d)\n", fname, strerror(err), err);
	}
	if (fp != NULL) {
		char* buf;
		size_t len;
		int ret = readall(fp, &buf, &len);
		fclose(fp);
		if (ret == READALL_OK) {
			if (buf) {
				out = buf;
				free(buf);
			}
			return len;
		}
	}
    return -1;
}

int32_t get_thread_id() noexcept {
    static int32_t thread_idx = 0;
    static std::mutex thread_mutex;
    static std::unordered_map<std::thread::id, int32_t> thread_ids;

    std::lock_guard<std::mutex> lock(thread_mutex);
    std::thread::id id = std::this_thread::get_id();
    auto iter = thread_ids.find(id);
    if (iter == thread_ids.end()) {
        iter = thread_ids.insert(std::pair<std::thread::id, int32_t>(id, thread_idx++)).first;
    }
    return iter->second;
}
