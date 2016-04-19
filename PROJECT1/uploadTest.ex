#!/usr/bin/expect
#REQUIRES expect app

spawn sftp eliot2@gl.umbc.edu
expect "password:"
send "mew2Rulesm\n"
expect "sftp>"
send "put babbler.c \n"
send "put babbler-test.c \n"
send "exit\n"
spawn ssh eliot2@gl.umbc.edu
expect "password:"
send "mew2Rulesm\n"
expect "%"
send "submit cs421_jtang proj1 babbler-test.c\n"
expect ":"
send "y\n"
send "exit\n"
interact
