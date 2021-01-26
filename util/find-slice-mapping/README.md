# Reverse engineer slice mapping function

TODO: write better instructions for this mini-program.

This code uses the approach from Maurice et al. (Section 4.3 of "Reverse Engineering Intel Last-Level Cache Complex Addressing Using Performance Counters").
To reproduce: uncomment the lines to print the binary representation of each address and the actual slice in the function `get_cache_slice_index()` of `util-cpu-specific.c`.
Compile `allocate-and-print.c` (in this find-slice-mapping folder).
Tune the offset between addresses (a low offset will help us find lower bits of the address and a high one higher)
Then run `allocate-and-print` and save the output into a file with `> out.txt`.
Finally run the python program `find-slice-mapping-function.py` on `out.txt`.
With a bit of manual work you should be able reverse engineer the slice mapping function.
Repeat with other offsets to speed up the computation (like 0x100000).
NOTE that you may have to increase the size of the buffer to up to 16GB.
Also you may have to enable more huge pages with `echo 32768 | sudo tee /proc/sys/vm/nr_hugepages`.