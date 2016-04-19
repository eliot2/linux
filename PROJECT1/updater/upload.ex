#!/usr/bin/expect
#REQUIRES expect app

spawn sftp eliot2@gl.umbc.edu
expect "password:"
send "mew2Rulesm\n"
expect "sftp>"
send "put hw4.c \n"
send "exit\n"
spawn ssh eliot2@gl.umbc.edu
expect "password:"
send "mew2Rulesm\n"
expect "%"
send "submit cs421_jtang hw4 hw4.c\n"
expect ":"
send "y\n"
send "exit\n"
interact
