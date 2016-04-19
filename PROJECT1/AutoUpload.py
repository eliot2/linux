import os
import time

toUpload = "babbler.c"
toUploadAlso = "babbler-test.c"
fileTime = os.stat(toUpload).st_mtime
fileTimeAlso = os.stat(toUploadAlso).st_mtime

while True:
    time.sleep(10)
    if fileTime != os.stat(toUpload).st_mtime:
        fileTime = os.stat(toUpload).st_mtime
        print "BABBLER FILE UPDATE, UPLOAD!"
        os.system("expect uploadBabbler.ex")
        time.sleep(7)
        print "Babbler uploaded."
    elif fileTimeAlso != os.stat(toUploadAlso).st_mtime:
        fileTimeAlso = os.stat(toUploadAlso).st_mtime
        print "BABBLER-TEST UPDATE, UPLOAD!"
        os.system("expect uploadTest.ex")
        time.sleep(7)
        print "Babbler-test uploaded."
    else:
        print "No new changes to upload."
