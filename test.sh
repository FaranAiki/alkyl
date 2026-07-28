gdb -q -ex "run" -ex "bt" -ex "quit" --args build/ethyl test/code/array/test_print_arr.aky &
GDB_PID=$!
sleep 1
kill -INT $GDB_PID
wait $GDB_PID
