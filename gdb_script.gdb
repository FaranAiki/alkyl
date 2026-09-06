set breakpoint pending on
b src/semantic/check.c:272
commands
  if g_wlr_output_events != 0
    watch ((SemSymbol*)g_wlr_output_events)->type.class_name
  end
  continue
end
run project/wmyl/wmyl.kyl -o build/out
bt
