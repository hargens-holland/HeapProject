////////////////////////////////////////////////////////////////////////////////
// Main File:        p3Heap part A
// This File:        p3Heap.c
// Other Files:      tests, Makefile
// Semester:         CS 354 Lecture 01 Fall 2024
// Grade Group:      gg7  (See canvas.wisc.edu/groups for your gg#)
// Instructor:       deppeler
//
// Author:           Holland Hargens
// Email:            hhargens@wisc.edu
// CS Login:         hhargens
//
///////////////////////////  OPTIONAL WORK LOG  //////////////////////////////
//	Completed alloc() function 10/18
//  Complete free_block() function 10/24
///////////////////////// OTHER SOURCES OF HELP //////////////////////////////
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//
// Online sources:   avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of
//                   of any information you find.
//
// AI chats:         save a transcript and submit with project.
//////////////////////////// 80 columns wide ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020-2024 Deb Deppeler based on work by Jim Skrentny
// Posting or sharing this file is prohibited, including any changes.
// Used by permission SPRING 2024, CS354-deppeler
//
/////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "p3Heap.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader
{

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) heap blocks have blockHeaders that contain size and status bits
     * 3) free heap block contain a footer, but we can use the blockHeader
     *.
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits.
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     *
     * Start Heap:
     *  The blockHeader for the first block of the heap is after skip 4 bytes.
     *  This ensures alignment requirements can be met.
     *
     * End Mark:
     *  The end of the available memory is indicated using a size_status of 1.
     *
     * Examples:
     *
     * 1. Allocated block of size 24 bytes:
     *    Allocated Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 25
     *      If the previous block is allocated p-bit=1 size_status would be 27
     *
     * 2. Free block of size 24 bytes:
     *    Free Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 24
     *      If the previous block is allocated p-bit=1 size_status would be 26
     *    Free Block Footer:
     *      size_status should be 24
     */
    int size_status;

} blockHeader;

/* Global variable - DO NOT CHANGE NAME or TYPE.
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;

/* Size of heap allocation padded to round to nearest page size.
 */
int alloc_size;

/*
 * Additional global variables may be added as needed below
 * TODO: add global variables needed by your function
 */

/*
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block (payload) on success.
 * Returns NULL on failure.
 *
 * This function must:
 * - Check size - Return NULL if size < 1
 * - Determine block size rounding up to a multiple of 8
 *   and possibly adding padding as a result.
 *
 * - Use BEST-FIT PLACEMENT POLICY to chose a free block
 *
 * - If the BEST-FIT block that is found is exact size match
 *   - 1. Update all heap blocks as needed for any affected blocks
 *   - 2. Return the address of the allocated block payload
 *
 * - If the BEST-FIT block that is found is large enough to split
 *   - 1. SPLIT the free block into two valid heap blocks:
 *         1. an allocated block
 *         2. a free block
 *         NOTE: both blocks must meet heap block requirements
 *       - Update all heap block header(s) and footer(s)
 *              as needed for any affected blocks.
 *   - 2. Return the address of the allocated block payload
 *
 *   Return if NULL unable to find and allocate block for required size
 *
 * Note: payload address that is returned is NOT the address of the
 *       block header.  It is the address of the start of the
 *       available memory for the requesterr.
 *
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void *alloc(int size)
{
    // Ensure heap start was initialized
    if (heap_start == NULL)
    {
        return NULL;
    }
    // Validate size is greater than 1
    if (size < 1)
    {
        return NULL;
    }
    // Calculate total block size and ensure it's a multiple of 8
    int total_block_size = size + sizeof(blockHeader);
    if (total_block_size % 8 != 0)
    {
        total_block_size = ((total_block_size + 7) / 8) * 8; // Round up to nearest multiple of 8
    }

    // Iterate to find best fit
    blockHeader *current = heap_start;
    blockHeader *best_block = NULL;
    int best_block_size = alloc_size + 1;

    while (current->size_status != 1 && current < (blockHeader *)((char *)heap_start + alloc_size))
    {
        int current_block_size = current->size_status & ~3; // Mask out status bits

        if (!(current->size_status & 1)) // If block is free
        {
            if (current_block_size >= total_block_size)
            {
                // Check if this block is a better fit
                if (best_block == NULL || current_block_size < best_block_size)
                {
                    best_block = current;
                    best_block_size = current_block_size;
                }
            }
        }
        current = (blockHeader *)((char *)current + (current->size_status & ~3));
    }
    // Return NULL if no block of sufficient size found
    if (best_block == NULL)
    {
        return NULL;
    }

    int original_size = best_block->size_status & ~3;

    // Check if we should split the block
    if (original_size - total_block_size >= sizeof(blockHeader) + 8)
    {
        // Create the new free block
        blockHeader *new_free_block = (blockHeader *)((char *)best_block + total_block_size);
        new_free_block->size_status = (original_size - total_block_size) | 2; // Set previous as allocated

        // Set the footer for the new free block
        blockHeader *new_footer = (blockHeader *)((char *)new_free_block + (new_free_block->size_status & ~3) - sizeof(blockHeader));
        new_footer->size_status = new_free_block->size_status & ~3;

        // Update the size of the allocated block
        best_block->size_status = total_block_size | 1; // Mark as allocated
        if (best_block->size_status & 2)                // Preserve p-bit
            best_block->size_status |= 2;
    }
    else
    {
        // Otherwise mark the block as allocated
        best_block->size_status |= 1;
    }

    // Update next block's p-bit
    blockHeader *next_block = (blockHeader *)((char *)best_block + (best_block->size_status & ~3));
    if (next_block->size_status != 1)
        next_block->size_status |= 2; // if it isn't the end block Set p-bit

    return (void *)((char *)best_block + sizeof(blockHeader));
}

/*
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - Update header(s) and footer as needed.
 *
 * If free results in two or more adjacent free blocks,
 * they will be immediately coalesced into one larger free block.
 * so free blocks require a footer (blockHeader works) to store the size
 *
 * TIP: work on getting immediate coalescing to work after your code
 *      can pass the tests in partA and partB of tests/ directory.
 *      Submit code that passes partA and partB to Canvas before continuing.
 */
int free_block(void *ptr)
{
    // Check for null pointer
    if (ptr == NULL)
    {
        return -1;
    }

    blockHeader *block_ptr = (blockHeader *)((char *)ptr - sizeof(blockHeader));

    // Check if pointer is within heap bounds
    if ((void *)block_ptr < (void *)heap_start ||
        (void *)block_ptr >= (void *)((char *)heap_start + alloc_size))
    {
        return -1;
    }
    // Check if block address is 8-byte aligned
    if ((unsigned long)ptr % 8 != 0)
    {
        return -1;
    }
    // Check if block is already free
    if (!(block_ptr->size_status & 1))
    {
        return -1;
    }
    // Get the current block size
    int current_block_size = block_ptr->size_status & ~3;

    // Mark block as free
    block_ptr->size_status &= ~1;

    // Add footer to the free block
    blockHeader *footer = (blockHeader *)((char *)block_ptr + current_block_size - sizeof(blockHeader));
    footer->size_status = current_block_size;

    // Get next block
    blockHeader *next_block = (blockHeader *)((char *)block_ptr + current_block_size);
    if (next_block->size_status != 1)
    {
        // Clear the P-bit of next block
        next_block->size_status &= ~2;
    }
    // Coalesce with next block if it's free and not the end mark
    if (next_block->size_status != 1 && !(next_block->size_status & 1))
    {
        // Add next block's size to current block
        int next_size = next_block->size_status & ~3;
        current_block_size += next_size;
        block_ptr->size_status = current_block_size;
        if (block_ptr->size_status & 2)
        {
            block_ptr->size_status |= 2;
        }

        // Update footer after combining blocks
        footer = (blockHeader *)((char *)block_ptr + current_block_size - sizeof(blockHeader));
        footer->size_status = current_block_size;
    }

    // Coalesce with previous block if it's free
    if (!(block_ptr->size_status & 2))
    {
        // Get previous block's footer
        blockHeader *prev_footer = (blockHeader *)((char *)block_ptr - sizeof(blockHeader));
        int prev_size = prev_footer->size_status;

        // Get previous block's header
        blockHeader *prev_block = (blockHeader *)((char *)block_ptr - prev_size);

        // Calculate the total size after coalescing
        int total_size = prev_size + current_block_size;

        // Update previous block's size to include current block
        prev_block->size_status = total_size;
        if (prev_block->size_status & 2)
        {
            prev_block->size_status |= 2;
        }

        // Update footer
        footer = (blockHeader *)((char *)prev_block + total_size - sizeof(blockHeader));
        footer->size_status = total_size;
    }

    return 0;
}

/*
 * Initializes the memory allocator.
 * Called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int init_heap(int sizeOfRegion)
{

    static int allocated_once = 0; // prevent multiple myInit calls

    int pagesize;   // page size
    int padsize;    // size of padding when heap size not a multiple of page size
    void *mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader *end_mark;

    if (0 != allocated_once)
    {
        fprintf(stderr,
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0)
    {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S.
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd)
    {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr)
    {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader *)mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader *)((void *)heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader *)((void *)heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
}

/* STUDENTS MAY EDIT THIS FUNCTION, but do not change function header.
 * TIP: review this implementation to see one way to traverse through
 *      the blocks in the heap.
 *
 * Can be used for DEBUGGING to help you visualize your heap structure.
 * It traverses heap blocks and prints info about each block found.
 *
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts)
 * t_End    : address of the last byte in the block
 * t_Size   : size of the block as stored in the block header
 */
void disp_heap()
{

    int counter;
    char status[6];
    char p_status[6];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout,
            "********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout,
            "--------------------------------------------------------------------------------\n");

    while (current->size_status != 1)
    {
        t_begin = (char *)current;
        t_size = current->size_status;

        if (t_size & 1)
        {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        }
        else
        {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2)
        {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        }
        else
        {
            strcpy(p_status, "FREE ");
        }

        if (is_used)
            used_size += t_size;
        else
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status,
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader *)((char *)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout,
            "--------------------------------------------------------------------------------\n");
    fprintf(stdout,
            "********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout,
            "********************************************************************************\n");
    fflush(stdout);

    return;
}
