lines = open('project/wmyl/wmyl.kyl').readlines()
with open('project/wmyl/wmyl.kyl', 'w') as f:
    for line in lines:
        if 'let modifiers_ptr = &(' in line:
            f.write("    let modifiers_ptr = ((wlr_keyboard as long) + 280) as void*;\n")
        else:
            f.write(line)
