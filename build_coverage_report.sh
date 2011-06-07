#!/bin/sh
lcov --directoryProfiling --zerocounters
Profiling/tests/tests
lcov --directory Profiling --capture --output-file Profiling/app.info
genhtml -o Profiling/ Profiling/app.info
firefox Profiling/index.html
