# Custom memory allocator 

Author: Bharath Gajjala  

## About This Project
This is a custom memory allocator using mmap and munmap. This includes all best-fit, worst-fit,and first-fit algorithm application.

### Program startup
Startup: Make to compile the files. 

To use (one specific command):
LD_PRELOAD=$(pwd)/allocator.so command  
('command' will run with your allocator)

To use (all following commands):                                                                  
export LD_PRELOAD=$(pwd)/allocator.so  
(Everything after this point will use your custom allocator -- be careful!) 

### Included Files
Files included:
   - <b>Makefile</b>: Including to compile and run the program.
   - <b>allocator.c</b>: The custom allocator which hold all the functions.

Header files for files are included


To compile and run:

```bash
make
```
### Example allocation
```bash
$ LD_PRELOAD=./allocator.so ls
allocator.c  allocator.h  allocator.h.gch  allocator.so  a.out  docs  liballocator.so  logger.h  logger.h.gch  Makefile  README.md  tests
```

## Logging

By default logging is turned off. In order to turn on change option in Makefile.

