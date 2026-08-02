#include "linker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char* alkyl_get_linker_command(LinkerType type) {
    switch (type) {
        case LINKER_ALYNK:  return "echo not implemented";
        case LINKER_CLANG:  return "clang -g -O0 %s -o %s -no-pie %s";
        case LINKER_LLD:    return "ld.lld -o %s %s %s -no-pie";
        case LINKER_MOLD:   return "mold -o %s %s %s";
        case LINKER_GCC:
        default:            return "gcc -g -O0 %s -o %s -no-pie %s";
    }
}

int alkyl_link(const char *obj_file, const char *output_basename, const char *link_flags, LinkerType linker_type) {
    if (linker_type == LINKER_NONE) return 0;
    
    char stamp_file[1024];
    snprintf(stamp_file, sizeof(stamp_file), "%s.link_stamp", output_basename);

    struct stat obj_st;
    if (stat(obj_file, &obj_st) != 0) {
        return -1;
    }

    FILE *stamp = fopen(stamp_file, "r");
    if (stamp) {
        long long saved_mtime = 0;
        if (fscanf(stamp, "%lld", &saved_mtime) == 1 && saved_mtime == obj_st.st_mtime) {
            fclose(stamp);
            return 0;
        }
        fclose(stamp);
    }

    char cmd[2048];
    const char *cmd_fmt = alkyl_get_linker_command(linker_type);
    snprintf(cmd, sizeof(cmd), cmd_fmt, obj_file, output_basename, link_flags ? link_flags : "");

    int res = system(cmd);
    if (res != 0) {
        return res;
    }

    stamp = fopen(stamp_file, "w");
    if (stamp) {
        fprintf(stamp, "%lld\n", (long long)obj_st.st_mtime);
        fclose(stamp);
    }

    return 0;
}
