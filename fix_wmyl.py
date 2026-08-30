with open("project/wmyl/wmyl.kyl", "r") as f:
    lines = f.readlines()

new_lines = []
skip = False
for i, line in enumerate(lines):
    # Remove @c import block
    if line.startswith("@c import \"wlr/backend.h\""):
        new_lines.append('import "wlroots/core";\n')
        skip = True
    if skip and line.startswith("@c import \"wayland-server-core.h\""):
        skip = False
        continue
    
    if skip:
        continue
        
    # Remove export namespace wlroots { but keep contents from class Display {
    if line.startswith("export namespace wlroots {"):
        skip = True
    
    if skip and line.strip().startswith("class Display {"):
        skip = False
        new_lines.append(line)
        continue

    if not skip:
        # If this is the closing brace for the namespace wlroots (line 330)
        if line.strip() == "}" and i < len(lines)-1 and lines[i+1].strip() == "" and lines[i+2].startswith("class Server {"):
            continue
        new_lines.append(line)

with open("project/wmyl/wmyl.kyl", "w") as f:
    f.writelines(new_lines)
