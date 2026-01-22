gcc -O0 burn.c -o burn
sudo timeout 2s prlimit --rttime=unlimited chrt -f 99 taskset -c 0 ./burn