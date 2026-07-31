import os

files_to_update = [
    "./TODO.md",
    "./lib/c.hky",
    "./.gitignore",
    "./docs/business-arch/05_requirement_analysis_and_design_definition.md",
    "./docs/business-arch/03_requirement_life_cycle_management.md",
    "./scripts/run_tests.sh",
    "./src/parser/core.c",
    "./src/driver/cli.c",
    "./src/driver/main.c",
    "./test/code/purge/test_warning.kyl",
    "./test/code/array/test_print_arr.kyl",
    "./test/code/builtin/test_typeof.kyl",
    "./test/code/print/test_meta_print.kyl",
    "./test/code/misc/test_print.kyl",
    "./misc/installer/linux/install.sh",
    "./misc/installer/linux/debian/rules",
    "./misc/installer/linux/arch/PKGBUILD",
    "./misc/installer/windows/install_associations.bat",
    "./misc/vim/ftdetect/alkyl.vim",
    "./README.md"
]

for filepath in files_to_update:
    if not os.path.exists(filepath):
        continue
    with open(filepath, 'r') as f:
        content = f.read()
    
    new_content = content.replace(".aky", ".kyl")
    # if it's the vim file, also replace aky with kyl for the extension
    if filepath == "./misc/vim/ftdetect/alkyl.vim":
        new_content = new_content.replace("*.aky", "*.kyl")
    
    with open(filepath, 'w') as f:
        f.write(new_content)

print("Done replacing.")
