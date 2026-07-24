import re

tests = [
    "test/code/misc/test.aky",
    "test/code/purge/test_purge.aky"
]

for t in tests:
    try:
        with open(t, 'r') as f:
            content = f.read()
            
        if "void main" in content:
            content = content.replace("void main", "int main")
            # insert return 0; before the last brace
            idx = content.rfind("}")
            if idx != -1:
                content = content[:idx] + "  return 0;\n" + content[idx:]
            with open(t, 'w') as f:
                f.write(content)
    except:
        pass
