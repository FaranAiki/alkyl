import re

with open("include/lexer/lexer.h", "r") as f:
    content = f.read()

tokens_to_add = ["TOKEN_COVALENT", "TOKEN_INFMUT", "TOKEN_PREMUT", "TOKEN_SUFMUT", "TOKEN_INFOP", "TOKEN_PREFOP", "TOKEN_SUFFOP"]
for t in tokens_to_add:
    if t not in content:
        content = content.replace("TOKEN_UNKNOWN", f"{t},\n  TOKEN_UNKNOWN")

# Now update the keywords array
# find where keywords start and end
start_idx = content.find("static const KeywordDef keywords[] = {")
end_idx = content.find("};", start_idx)

keywords_block = content[start_idx:end_idx]
import ast
lines = keywords_block.split("\n")[1:] # skip first line
existing_kws = []
for line in lines:
    line = line.strip()
    if line.startswith("{") and "}" in line:
        k = line[line.find('"')+1 : line.rfind('"')]
        v = line[line.find(',')+1 : line.find('}')].strip()
        existing_kws.append((k, v))

# The previous edits might have messed up the existing_kws list, let's just make sure all of them are parsed properly
# Wait, let's just rewrite the whole keywords block to be sure
# Actually, I can just use a regex to extract all string-token pairs
pairs = re.findall(r'\{"([^"]+)",\s*([^}]+)\}', keywords_block)

# Add our new ones
new_kws = {
    "covalent": "TOKEN_COVALENT",
    "infmut": "TOKEN_INFMUT",
    "premut": "TOKEN_PREMUT",
    "sufmut": "TOKEN_SUFMUT"
}

for k, v in new_kws.items():
    found = False
    for i, (ext_k, ext_v) in enumerate(pairs):
        if ext_k == k:
            found = True
            break
    if not found:
        pairs.append((k, v))

# Sort alphabetically
pairs.sort(key=lambda x: x[0])

# Reconstruct block
new_block = "static const KeywordDef keywords[] = {\n"
for k, v in pairs:
    new_block += f'    {{"{k}", {v}}},\n'
    
content = content[:start_idx] + new_block + content[end_idx:]

with open("include/lexer/lexer.h", "w") as f:
    f.write(content)

print("lexer.h updated successfully.")
