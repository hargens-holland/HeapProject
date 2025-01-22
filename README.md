# p3HeapMemoryAllocator

This project implements a custom memory allocator in C, designed to manage heap memory efficiently using a best-fit allocation strategy. It includes functionality for allocating and freeing memory blocks, as well as tools for debugging and visualizing heap usage.

## Features

- **Best-fit Allocation**: Allocates memory blocks using a best-fit policy to minimize fragmentation.
- **Dynamic Memory Management**:
  - Allocate memory using the `alloc()` function.
  - Free memory and coalesce adjacent free blocks using `free_block()`.
- **Heap Visualization**: A `disp_heap()` function displays the current state of the heap for debugging purposes.

## Files

- `p3Heap.c`: Main implementation file containing the memory allocation functions.
