gdb --batch \
  -ex "b src/semantic/check.c:254" \
  -ex "run" \
  -ex "p cn->name" \
  -ex "p existing_has_body" \
  -ex "c" \
  --args build/alkyl project/wmyl/wmyl.kyl
