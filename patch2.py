import sys

for file in ['src/meta/eval_math.c', 'src/meta/eval_misc.c']:
    with open(file, 'r') as f:
        content = f.read()
    content = content.replace('.as.double_val', '.as.single_val')
    with open(file, 'w') as f:
        f.write(content)

