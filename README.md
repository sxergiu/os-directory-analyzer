# Directory Analyzer

## Overview
**Directory Analyzer** is a C program developed during an Operating Systems class. The program monitors directories provided as arguments, detecting changes or deletions by creating file snapshots in a specified output directory. It also detects potentially dangerous files via a Bash script and quarantines them into an isolated directory.

## Features
- Detects changes and deletions in directories by creating snapshots of all files in an output directory.
- Quarantines potentially dangerous files detected by a Bash script in an isolated directory.
- Each directory is analyzed in a separate child process.
- Each dangerous file is handled in its own child process.

## How It Works
1. The program analyzes directories provided as arguments.
2. Snapshots of the files in these directories are stored in a specified output directory.
3. A Bash script scans for potentially dangerous files.
4. Detected files are moved to a specified isolated directory for quarantine.

## Setup and Usage
1. Clone the repository.
2. Compile the C program:
   ```bash
   gcc -o directory_analyzer directory_analyzer.c
 3. Running the program:
  ./directory_analyzer /path/to/outputdir /path/to/isolateddir /path/to/dir1 /path/to/dir2 /path/to/dir3 ...
