Praq4.c (c) 2022

This is modified version of praq3 from 2010.

This version doesn't use the ftell() function which has limitations on file sizes > 4GB.
Praq3 can correctly compress and decompress files > 4GB but outputs on screen sizes of only < 4GB.
Praq4 uses the new gtbitio2.c and gtbitio2.h, and ucodes2.c and ucodes2.h. 
Gtbitio2 records nbytes_out and nbytes_read number of bytes written or read.
So this praq4 can output on screen the correct file sizes and also for showing compression ratio after encoding.

This is how will my new compression programs will look like, if there would be more.

Gerald R Tamayo (c) 2022, geraldrtamayo@yahoo.com
