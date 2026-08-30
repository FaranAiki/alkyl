with open('lib/wlroots/core.kyl', 'r') as f:
    text = f.read()
print("Has @c import inside export namespace? ", text.find('@c import') > text.find('export namespace wlroots {'))
