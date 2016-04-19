import os
import time

toUpload = "hw4.c"
fileTime = os.stat(toUpload).st_mtime

while True:
    time.sleep(10)
    if fileTime != os.stat(toUpload).st_mtime:
        fileTime = os.stat(toUpload).st_mtime
        print "FILE UPDATE, UPLOAD!"
        os.system("expect upload.ex")
        time.sleep(7)
        print "File uploaded."
    else:
        print "No new changes to upload."
