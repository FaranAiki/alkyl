#include "common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_LIBZIP
#include <zip.h>

static int is_alkyl_source(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 1;
    if (streq(ext, ".kyl")) return 1;
    if (streq(ext, ".hky")) return 1;
    if (streq(ext, ".alk")) return 1;
    if (streq(ext, ".alky")) return 1;
    if (streq(ext, ".alkyl")) return 1;
    if (streq(ext, ".aky")) return 1;
    return 0;
}

char* read_zip_file(const char *path) {
    int err = 0;
    zip_t *za = zip_open(path, ZIP_RDONLY, &err);
    if (!za) {
        return NULL;
    }

    zip_int64_t num_entries = zip_get_num_entries(za, 0);
    char *result = NULL;

    for (zip_uint64_t i = 0; i < (zip_uint64_t)num_entries; i++) {
        const char *name = zip_get_name(za, i, 0);
        if (!name) continue;
        if (name[0] == '_' && name[1] == '_' && name[2] == 'M') continue;
        if (!is_alkyl_source(name)) continue;

        zip_stat_t st;
        if (zip_stat_index(za, i, 0, &st) != 0 || st.size > SIZE_MAX - 1) continue;

        zip_file_t *zf = zip_fopen_index(za, i, 0);
        if (!zf) continue;

        char *buf = malloc((size_t)st.size + 1);
        if (!buf) {
            zip_fclose(zf);
            zip_close(za);
            return NULL;
        }

        zip_int64_t nread = zip_fread(zf, buf, st.size);
        zip_fclose(zf);

        if (nread < 0 || (zip_uint64_t)nread != st.size) {
            free(buf);
            continue;
        }

        buf[st.size] = 0;
        result = buf;
        break;
    }

    zip_close(za);
    return result;
}

#else
/* Mom, do we have a libzip at home? Libzip at home: */
char *read_zip_file(const *char path) {
    return 0;
}

#endif // HAVE_LIBZIP
