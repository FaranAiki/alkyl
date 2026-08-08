import re
with open("CMakeLists.txt", "r") as f:
    text = f.read()
text = text.replace("EXACT", "")
with open("CMakeLists.txt", "w") as f:
    f.write(text)
