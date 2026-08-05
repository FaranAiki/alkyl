sed -i 's/char \*bracket = strchr(node->var_type.class_name, '"'"'\\['"'"');/char *bracket = strchr(node->var_type.class_name, '"'"'\\['"'"'); char *angle = strchr(node->var_type.class_name, '"'"'<'"'"');/' src/semantic/type.c
sed -i 's/if (bracket) {/if (bracket || angle) {/' src/semantic/type.c
sed -i 's/if (mangled\[i\] == '"'"'\\['"'"') mangled\[i\] = '"'"'_'"'"';/if (mangled[i] == '"'"'\\['"'"' || mangled[i] == '"'"'<'"'"') mangled[i] = '"'"'_'"'"';/' src/semantic/type.c
sed -i 's/else if (mangled\[i\] == '"'"'\\]'"'"') mangled\[i\] = '"'"'\\0'"'"';/else if (mangled[i] == '"'"'\\]'"'"' || mangled[i] == '"'"'>'"'"') mangled[i] = '"'"'\\0'"'"';/' src/semantic/type.c
