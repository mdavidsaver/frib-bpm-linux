FRIB BPM linux driver
=====================

Stub driver for BPM.
Maps BAR0 and provides 100Hz capture buffer as simulated BAR1.

Size of capture buffer, and offset in BAR0, presently hard coded.


```sh
git clone https://github.com/mdavidsaver/frib-bpm-linux.git
cd frib-bpm-linux
make
insmod frib_bpm.ko
```
