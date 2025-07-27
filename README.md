# Storage Class Memory Manager (SCM)

A C library and interactive shell for **persistent memory allocation** backed by a file.  
It provides a `malloc`‑like API over a memory‑mapped region and an AVL‑tree allocator, plus a CLI to insert, remove, and inspect stored data.

---

## 🚀 Key Features

- **Persistent Allocation API**  
  - `scm_open(pathname, truncate)` – open or initialize a backing file  
  - `scm_malloc(scm, n)` – allocate `n` bytes in SCM region  
  - `scm_free(scm, ptr)` – free a previously allocated block  
  - `scm_close(scm)` – flush and close the region  

- **Interactive Shell**  
  - Commands: `insert <word>`, `remove <word>`, `exists <word>`, `load <file>`, `list`, `info`, `help`, `quit`  
  - Built‑in history and trimming via `term.c` / `shell.c`  

- **AVL‑Tree Allocator**  
  - Tracks allocations in an AVL tree (`avl.c` / `avl.h`)  
  - Efficient lookups for existence, insertion, and removal  

- **Memory‑Mapped Backing**  
  - Uses `mmap`, `msync`, and file I/O (`scm.c` / `scm.h`)  
  - Falls back to `sbrk` if needed, with file‑truncation support  

---

## 🛠️ Tech Stack

- **Language:** C  
- **Build:** GNU Make, GCC (`-ansi -pedantic -Wall -O3`)  
- **Modules:**  
  - `scm.c` / `scm.h` – SCM API and file mapping  
  - `avl.c` / `avl.h` – AVL‑tree metadata management  
  - `system.c` / `system.h` – utilities (`us_sleep`, file helpers)  
  - `term.c` / `term.h` – terminal I/O helpers  
  - `shell.c` / `shell.h` – CLI parsing and dispatch  
  - `main.c` – combines SCM + shell into `cs238` executable  
- **Extras:** `.vscode` settings for easy debugging  

---

## ⚙️ Build & Run

```bash
# Build the library and CLI
make

# Usage
./cs238 [--truncate] [--nocolor] <backing-file>
