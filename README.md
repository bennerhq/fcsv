# fcsv - Fast CSV

`fcsv` (Fast CSV) is a tool for working with large CSV files. It provides a simple and efficient 
way to read, manipulate, and write any size CSV files.

It's written in native C, with very limited dependencies, and with portability and speed in mind.

... Oh, and yes—I also wrote a small static typed scripting language, a compiler for it, and a
stack machine to execute the code!

## Background
Why this project? I wanted to understanding the capability of GitHub Co-pilot. code, I 
followed five core principles:

 1. Always start a coding solution by asking AI to generate the first draft of the code.
 2. All code must be written in standard C for optimal speed.
 3. Only use standard C libraries to ensure maximum platform portability.
 4. Always design for a low memory and resource footprint.
 5. Execution speed is the top priority in all algorithmic design choices.

## Features

- Efficiently handles large CSV files
- Provides functions for reading and writing CSV data
- Supports various CSV formats and delimiters
- Allows for easy manipulation of CSV data
- Output C code from the scripting language, that can replace the stack machine

## Code Functionality

The core functionality of `fcsv` is implemented in C, ensuring high performance and efficiency. 
The C code provides the following features:

- **Reading CSV Files**: Functions to read CSV files line by line or in bulk, handling 
different delimiters and formats.
- **Writing CSV Files**: Functions to write data to CSV files, supporting various 
delimiters and formats.
- **Data Manipulation**: Functions to manipulate CSV data, such as filtering rows, 
selecting columns, and transforming data.
- **Memory Management**: Efficient memory management to handle large datasets without 
excessive memory usage.

## TO-DO

- Improved handle of misformatted CSV files
- Improved error handeling in general
- Handle CSV with and without headline (cli controlled)
- Make scripting language turing-complete?

## Usage

FIXME!
