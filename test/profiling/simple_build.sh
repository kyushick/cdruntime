mpicxx -std=gnu++0x -Wall -g -O -fPIC -o test_profiling test_profiling.cc -I../../src -I../../plugin/wrapper -I../../src/persistence -lwrapLibc -lcd -lpacker -L../../lib -Wl,-rpath=../../lib -Wl,-rpath=../../plugin/wrapper -Wl,-rpath=../../src/persistence -L../../src/persistence -L../../plugin/wrapper -lc
