import os
import shutil
import json
import glob

def categorize_file(path):
    with open(path, 'r') as f:
        content = f.read()
    
    keywords = ["union", "class", "sizeof", "as", "extern", "namespace", "import", "pure", "mut", "if", "while", "for", "switch"]
    found = []
    for k in keywords:
        if k in content:
            found.append(k)
            
    if "union" in found: return "union"
    if "sizeof" in found: return "sizeof"
    if "as" in found and "class" in found: return "cast"
    if "class" in found: return "class"
    if "namespace" in found: return "namespace"
    if "extern" in found: return "extern"
    
    # Fallback to general based on name
    name = os.path.basename(path).lower()
    for k in keywords:
        if k in name:
            return k
    return "general"

os.makedirs("test/code/features", exist_ok=True)

files_to_move = []
for root, dirs, files in os.walk("test/code"):
    if "features" in root: continue
    for file in files:
        if file.endswith(".aky"):
            files_to_move.append(os.path.join(root, file))

for f in glob.glob("test_*.aky"):
    files_to_move.append(f)
for f in glob.glob("test_*.c"):
    files_to_move.append(f)

for f in files_to_move:
    if f.endswith(".aky"):
        cat = categorize_file(f)
    else:
        # It's a .c file, move with its .aky counterpart
        cat = "general"
        base = os.path.basename(f).replace(".c", ".aky")
        if os.path.exists(base): cat = categorize_file(base)
        
    dest_dir = os.path.join("test/code/features", cat)
    os.makedirs(dest_dir, exist_ok=True)
    shutil.move(f, os.path.join(dest_dir, os.path.basename(f)))

# Also create howto.json schemas
schemas = {
    "union": {
        "keyword": "union",
        "description": "Defines a union type where all fields share the same memory location.",
        "features": ["Zero initialization", "Memory sharing", "Sizeof calculation based on largest member"],
        "example": "union Value { int i; double d; }"
    },
    "class": {
        "keyword": "class",
        "description": "Defines a struct/class type containing fields and methods.",
        "features": ["Implicit constructor generation", "Method definition", "Trait composition (has)", "Inheritance (is)"],
        "example": "class A is B has C { int x; }"
    },
    "sizeof": {
        "keyword": "sizeof",
        "description": "Calculates the memory size in bytes of a given type.",
        "features": ["Works with basic types (int, float)", "Works with composite types (class, union)", "Pointer size calculation"],
        "example": "sizeof(int)"
    },
    "cast": {
        "keyword": "as",
        "description": "Performs type casting between compatible types.",
        "features": ["Primitive casting (int as float)", "Pointer casting (A* as void*)", "Class reference to void* casting"],
        "example": "ptr as void*"
    },
    "extern": {
        "keyword": "extern",
        "description": "Declares an external C function or variable.",
        "features": ["C ABI linking", "Variadic arguments support (...)"],
        "example": "extern int printf(string format, ...);"
    },
    "namespace": {
        "keyword": "namespace",
        "description": "Groups functions and classes into a logical namespace block.",
        "features": ["Scope isolation", "Symbol name mangling prefix"],
        "example": "namespace Math { pure int add(int a, int b) { return a+b; } }"
    },
    "general": {
        "keyword": "general",
        "description": "General statements and expressions in Alkyl.",
        "features": ["Assignments", "Arithmetic", "Control flow"],
        "example": "let mut x = 1;"
    }
}

for cat in os.listdir("test/code/features"):
    cat_dir = os.path.join("test/code/features", cat)
    if os.path.isdir(cat_dir):
        schema = schemas.get(cat, schemas["general"])
        with open(os.path.join(cat_dir, "howto.json"), "w") as f:
            json.dump(schema, f, indent=4)

print("Refactoring complete.")
