# CaDiCaL User Propagator template project

Quick, mostly generated, template project for using CaDiCaL's user propegator. Checked against the larger template project given to me by Mathias Fleury to make sure all is good.

Build and run with:

```sh
cmake -B build && cmake --build build
./build/propagator_demo
```

### Submodule help

To pull everything in properly use:
```shell
git pull && git submodule init && git submodule update && git submodule status
```

To pull submodules:
```sh
git submodule update --remote --merge
```
