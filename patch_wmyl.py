lines = open('project/wmyl/wmyl.kyl').readlines()
with open('project/wmyl/wmyl.kyl', 'w') as f:
    for line in lines:
        if 'approximate offset' in line:
            f.write("    let modifiers_ptr = &((wlr_keyboard as wlroots.wlr_keyboard*).modifiers) as void*;\n")
        elif 'enum wlr_input_device_type is at offset 0' in line:
            f.write("    let type = (device as wlroots.wlr_input_device*).type;\n")
        else:
            f.write(line)
