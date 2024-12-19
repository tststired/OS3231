# 3121OS

Implemented a software bridge between a set of file-related system calls inside the OS/161 kernel and their implementation within the Virtual file system. Operating system will be able to run a single application at user-level and perform some basic file I/O.

Implemented the virtual memory sub-system of OS/161. The existing VM implementation in OS/161, dumbvm, is a minimal implementation with a number of shortcomings. Adapt OS/161 to take full advantage of the simulated hardware by implementing management of the MIPS software-managed Translation Lookaside Buffer (TLB). Main code used to handle TLB and system memory 

Most understated statement from the assignment specs

"A substantial part of this assignment is understanding how OS/161 works and determining what code is required to implement the required functionality. Expect to spend at least as long browsing and digesting OS/161 code as actually writing and debugging your own code."
