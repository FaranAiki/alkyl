import glob

for f in glob.glob('src/parser/**/*.c', recursive=True):
    with open(f, 'r') as file:
        content = file.read()
    
    if "p->recover_buf" in content:
        # Some might return NULL, some might return other values. We'll try to just return whatever is appropriate.
        # But wait, parser_fail(p, ...) will set p->has_error = 1. The original code used parser_fail which printed the error,
        # then if (p->recover_buf) longjmp(...).
        # We can just change `if (p->recover_buf) longjmp(*p->recover_buf, 1);` to `return NULL;` because p->has_error is already set by parser_fail!
        content = content.replace("if (p->recover_buf) longjmp(*p->recover_buf, 1);", "return NULL;")
        
        with open(f, 'w') as file:
            file.write(content)

