with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

old_code = """                for (int vi = 0; vi < 5 && ncand < 14; vi++) {
                    snprintf(cand_versioned[0], sizeof(cand_versioned[0]), "wlroots-%s", versions[vi]);
                    candidates[ncand++] = cand_versioned[0];
                    if (vi == 0) {
                        snprintf(cand_versioned[1], sizeof(cand_versioned[1]), "wlroots-%s", versions[vi]);
                    }
                }"""

new_code = """                static char cand_versioned_bufs[5][32]; // Use separate buffers
                for (int vi = 0; vi < 5 && ncand < 14; vi++) {
                    snprintf(cand_versioned_bufs[vi], sizeof(cand_versioned_bufs[vi]), "wlroots-%s", versions[vi]);
                    candidates[ncand++] = cand_versioned_bufs[vi];
                }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    print("Fixed candidates loop")
else:
    print("Could not find old_code")

old_code2 = """                size_t pkg_len = fread(pkg_out, 1, sizeof(pkg_out) - 1, pf);
                pkg_out[pkg_len] = '\\0';
                pclose(pf);
                if (pkg_len == 0) continue;"""

new_code2 = """                size_t pkg_len = fread(pkg_out, 1, sizeof(pkg_out) - 1, pf);
                pkg_out[pkg_len] = '\\0';
                pclose(pf);
                
                printf("DEBUG: probing %s -> out='%s'\\n", candidates[ci], pkg_out);
                if (pkg_len == 0) continue;"""

if old_code2 in content:
    content = content.replace(old_code2, new_code2)
    print("Added probing debug")
else:
    print("Could not find old_code2")

old_code3 = """    } else {
        snprintf(cmd, sizeof(cmd), "echo '#include <%s>' | gcc -E -DWLR_USE_UNSTABLE -I. -xc - 2>/dev/null", fname);"""

new_code3 = """    } else {
        snprintf(cmd, sizeof(cmd), "echo '#include <%s>' | gcc -E -DWLR_USE_UNSTABLE -I. %s -xc - 2>/dev/null", fname, include_flags);"""

if old_code3 in content:
    content = content.replace(old_code3, new_code3)
    print("Added include_flags to cmd")
else:
    print("Could not find old_code3")

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)
