/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"
#include <string.h>

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */
/* research the above Needed API and design accordingly */

#define VM_ADDR 0X600000000000
#define SCM_SIGNATURE 0xDEEDBEED  /* Expected signature to identify SCM files */
#define META_SIZE 24  /* Size of the metadata area */

/* Metadata structure for SCM */
struct metadata {
    size_t size;         /* Utilized size in bytes */
    size_t signature;    /* Signature to identify the SCM file */
    size_t checksum;     /* Checksum to verify metadata integrity */
};

struct scm
{
    int fd;
    void *mem;
    size_t size;
    size_t length;
};

/* Function to calculate checksum */
uint64_t calculate_checksum(const struct metadata *meta) {
    return meta->size ^ meta->signature;
}

/* Function to initialize metadata for a new SCM file */
void initialize_metadata(struct scm *scm) {
    struct metadata *meta = (struct metadata *)scm->mem;

    /* Initialize metadata fields */
    meta->size = 0;
    meta->signature = SCM_SIGNATURE;
    meta->checksum = calculate_checksum(meta);  /* Set initial checksum */

    /* Update scm struct */
    scm->size = 0;
}

/* Function to validate and load metadata from an existing SCM file */
int load_metadata(struct scm *scm) {
    struct metadata *meta = (struct metadata *)scm->mem;

    /* Validate signature */
    if (meta->signature != SCM_SIGNATURE) {
        TRACE("Error: Invalid SCM signature");
        return -1;  /* Indicate failure */
    }

    /* Validate checksum */
    if (meta->checksum != calculate_checksum(meta)) {
        TRACE("Error: Metadata checksum validation failed");
        return -1;  /* Indicate failure */
    }

    /* Load the utilized size from metadata */
    
    scm->size = meta->size;
    return 0;  /* Indicate success */
}

void store_metadata(struct scm *scm) {
    struct metadata *meta = (struct metadata *)scm->mem;  /* Get pointer to metadata */

    /* Update the size in metadata */
    meta->size = scm->size;
    /* Recalculate checksum */
    meta->checksum = calculate_checksum(meta);  /* Ensure the checksum is valid */

    /* The changes will be automatically reflected in the memory-mapped region */
    /* due to the usage of MAP_SHARED. So no additional flush is required. */
    /*TRACE("Metadata updated successfully\n");*/
}

size_t file_size(struct scm * scm){
    struct stat st;
    size_t page;
    fstat(scm->fd,&st);
    if(S_ISREG(st.st_mode))
    {
        scm->length= st.st_size;
        page= page_size();
        scm->length = (scm->length/page) *page;
        
    }
    else {
        TRACE("Error: File is not a regular file");
    }
    return (scm->length)?-1:0;
}

int init_zero(const char *filename, size_t size) {
    int fd = open(filename, O_WRONLY);
    char buffer[4096];
    size_t bytes_written = 0;
    if (fd == -1) {
        return -1;  
    }
    
    memset(buffer, 0, sizeof(buffer));
    
    
    while (bytes_written < size) {
        ssize_t result = write(fd, buffer, sizeof(buffer));
        if (result == -1) {
            close(fd);
            return -1;  
        }
        bytes_written += result;
    }
    close(fd);
    return 0;
}

struct scm *scm_open(const char *pathname, int truncate) {
    long curr, vm_addr;
    size_t page;
    size_t fs;
    struct scm *scm;
    
    page = page_size();  /* Get the system's page size */
    scm = malloc(sizeof(struct scm));
    if (!scm) {
        TRACE("Error: Memory allocation failed for SCM structure");
        return NULL;
    }
    
    /* Initialize the SCM structure to zero */
    memset(scm, 0, sizeof(struct scm));

    scm->fd = open(pathname, O_RDWR);
    if (scm->fd == -1) {
        TRACE("Error: Failed to open file");
        free(scm);
        return NULL;
    }

    fs = file_size(scm);
    if (fs == 0) {
        TRACE("Error: File open or file size retrieval failed");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    /* Get the current program break (end of heap) */
    curr = (long)sbrk(0);

    /* Align the virtual memory address to the page size */
    vm_addr =(VM_ADDR/page)* page;
    if (vm_addr < curr) {
        TRACE("Error: Specified virtual memory address is below the current program break.");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (truncate) {
        struct stat st;
        /* Zero out the file content if truncating */
        if (fstat(scm->fd, &st) == -1 || init_zero(pathname, st.st_size) != 0) {
            TRACE("Error: Failed to initialize file content to zero");
            close(scm->fd);
            free(scm);
            return NULL;
        }
    }

    /* Map the memory, including the metadata section (first 24 bytes) */
    scm->mem = mmap((void *)vm_addr, scm->length, PROT_READ | PROT_WRITE, MAP_FIXED |  MAP_SHARED, scm->fd, 0);
    if (scm->mem == MAP_FAILED) {
        TRACE("Error: mmap failed");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (truncate) {
        initialize_metadata(scm);  /* Initialize metadata if truncating */
    } else {
        load_metadata(scm);        /* Load existing metadata */
    }

    /* Adjust `scm->mem` to point just after metadata */
    scm->mem = (void *)((char *)scm->mem + META_SIZE);

    return scm;
}

void scm_close(struct scm *scm)
{
    scm->mem = (void *)((char *)scm->mem - META_SIZE); 
    store_metadata(scm);
    if (scm->mem != NULL && scm->mem != MAP_FAILED)
    {
         /* Decrement `scm->mem` back by the size of the metadata before syncing */
        msync(scm->mem,scm->length,MS_SYNC);
        munmap(scm->mem,scm->length);
    }
    if(scm->fd)
    {
        close(scm->fd);
    }
    free(scm);
}

void *scm_malloc(struct scm *scm, size_t n)
{
    void * p;
    if(scm->size+n<=scm->length)
    {
        p=(void *)((char *)scm->mem + scm->size);
        scm->size+=n;
        return p;
    }
    return NULL;
}

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t len;
    char *new_str;
    /*Check for NULL input string*/
    if (!s) {
        return NULL;
    }

    /*Calculate the length of the string, including the null terminator*/
     len = strlen(s) + 1;  /*+1 for the null terminator*/

    /*Use scm_malloc to allocate memory for the string*/
    new_str = scm_malloc(scm, len);
    if (!new_str) {
        return NULL;  /*Memory allocation failed*/
    }
    /*Copy the string into the newly allocated memory*/
    strcpy(new_str, s);

    return new_str;
}

void *scm_mbase(struct scm *scm)
{
    return scm->mem;
}



size_t scm_utilized(const struct scm *scm)
{
    return scm->size;
}

size_t scm_capacity(const struct scm *scm)
{
    return scm->length;
}

