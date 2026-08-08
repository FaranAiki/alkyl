with open("src/common/hashmap.c", "r") as f:
    content = f.read()

content = content.replace("streq(entries[idx].key, key)", "streq_lit(entries[idx].key, key)")

with open("src/common/hashmap.c", "w") as f:
    f.write(content)
