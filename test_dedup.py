import re

with open('src/alir/core.c', 'r') as f:
    code = f.read()

# Let's see if we can deduplicate strings.
