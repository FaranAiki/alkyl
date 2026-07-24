import os
import glob

tests = [
    "test/code/cast/test_cyclic.aky",
    "test/code/cast/test_inherit.aky",
    "test/code/cast/test_overload.aky",
    "test/code/class/test_member_rvalue.aky",
    "test/code/class/test_rvalue.aky",
    "test/code/extern/test_extern_alias.aky",
    "test/code/extern/test_nested_ns.aky",
    "test/code/extern/test_ns_extern.aky",
    "test/code/extern/test_undef_ns.aky",
    "test/code/if/ifif.aky",
    "test/code/misc/test.aky",
    "test/code/purge/test_purge.aky",
    "test/code/taint/fallback_eval.aky",
    "test/code/union/test_union_features.aky"
]

for t in tests:
    with open(t, 'r') as f:
        content = f.read()
    
    # We want to replace `return X;` in main with `print(X);\nreturn 0;`
    # Simple regex replacing `return (.*);` with `import std.print\n` (if not present) and `print (\1);\nreturn 0;` inside main
    import re
    if "import std.print" not in content:
        content = "import std.print\n" + content
    
    # We only want to replace the LAST return in the file (assuming it's in main)
    # Find all returns
    matches = list(re.finditer(r'return\s+(.+);', content))
    if matches:
        last = matches[-1]
        val = last.group(1).strip()
        if val != '0':
            new_content = content[:last.start()] + f'print ({val});\n  return 0;' + content[last.end():]
            with open(t, 'w') as f:
                f.write(new_content)

