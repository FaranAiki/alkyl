import sys

with open('lib/std/vector.kyl', 'r') as f:
    text = f.read()

text = text.replace("if (this.capacity == 0) new_capacity = 1;", "if (this.capacity == 0) { new_capacity = 1; }")
text = text.replace("else new_capacity = this.capacity * 2;", "else { new_capacity = this.capacity * 2; }")
text = text.replace("if (index > this.size) return;", "if (index > this.size) { return; }")

with open('lib/std/vector.kyl', 'w') as f:
    f.write(text)
