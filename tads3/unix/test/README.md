# TADS 3 Unix Test Suite

To run the tests, first do a `make all tests` from the `unix/` directory.
If all goes well, type `make install` (the library files must be installed before the tests can run successfully).
Then enter `unix/test/` (this directory) and type `run_all_tests`.

Test output will be placed in the `test/out/` directory.
You should check up on any `.diff` files which are placed there; they indicate a mismatch between the test log and the pre-generated log.

&lt;<tril@igs.net>&gt;

<!-- EOF -->
