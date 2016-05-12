import os
import time

toUpload = "babbler2.c"
toUploadAlso = "babbler2-test.c"
fileTime = os.stat(toUpload).st_mtime
fileTimeAlso = os.stat(toUploadAlso).st_mtime

waitTime = 60

while True:
    if fileTime != os.stat(toUpload).st_mtime:
        fileTime = os.stat(toUpload).st_mtime
        print "BABBLER2 FILE UPDATE, UPLOAD!"
        os.system("expect uploadBabbler.ex")
        time.sleep(7)
        print "Babbler2 uploaded."
    elif fileTimeAlso != os.stat(toUploadAlso).st_mtime:
        fileTimeAlso = os.stat(toUploadAlso).st_mtime
        print "BABBLER2-TEST UPDATE, UPLOAD!"
        os.system("expect uploadTest.ex")
        time.sleep(7)
        print "Babbler2-test uploaded."
    else:
        print "No new changes to upload."
    time.sleep(waitTime)
