#!/usr/bin/expect
spawn ./build/ethyl
expect "In \\\[0\\\]: "
send "namespace some_ns {\r"
expect "In \\\[0\\\]: "
send "    int a() {\r"
expect "In \\\[0\\\]: "
send "        return 0\r"
expect "In \\\[0\\\]: "
send "    }\r"
expect "In \\\[0\\\]: "
send "}\r"
expect "In \\\[1\\\]: "
send "\004"
expect eof
