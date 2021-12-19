#include "fileio.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "str_util.h"


String pathResources = ""; // read only app resource directory: C:/program files/daw
String pathUserdata = ""; // writable app directory: C:/users/user/appdata/daw/

String toResourcePath(String relPath) {
	return pathResources + relPath;
}
String toUserdataPath(String relPath) {
	return pathUserdata + relPath;
}

void setResourcePath(String cwd) {
	if (cwd.length() && (!StrEndsWith(cwd, "/") && !StrEndsWith(cwd, "\\")))
		cwd += "/";
	pathResources = cwd;
}
void setUserdataPath(String cwd) {
	if (cwd.length() && (!StrEndsWith(cwd, "/") && !StrEndsWith(cwd, "\\")))
		cwd += "/";
	pathUserdata = cwd;
    CreateDirectoryIfNotExists(pathUserdata);
}

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
int64_t ReadFileText(const String& filename, String& out, int resourceType) {
    String fileResPath;
    if (resourceType == 0) {
        fileResPath = toResourcePath(filename);
    } else {
        fileResPath = toUserdataPath(filename);
    }
	const char* fname = StringAsCStr(fileResPath);
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
