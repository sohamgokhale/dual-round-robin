# Dual Round Robin Matching (DRRM) for High Speed Switch
---

This repository contains a C code for simulating DRRM switch on [SIM : A Fixed Length Packet Simulator ](https://web.archive.org/web/20100719163607/http://klamath.stanford.edu/tools/SIM/)

The code is a modified version of the round robin algorithm which is distributed with SIM.

### How to use
---

- Copy `drr.c` and `drr.h` files to the `ALGORITHMS` directory inside the SIM Installation folder.
- Add `drr.c` and `drr.h` to `ALGORITHMS/makefile`
- Add "drr" and requried descriptions to the table file `ALGORITHMS/algorithmTable.h.`
- Go to `main/` and recompile the by command `make all`

### Testing
---

The code has not been rigorously tested and is still under testing.
