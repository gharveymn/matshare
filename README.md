# `matshare`

`matshare` is a subsystem for MATLAB R2008b to R2017b which provides functions for creating and operating on shared memory. It has mechanisms for automatic garbage collection, thread safe memory creation, and in-place overwriting.

Note: This is not available for R2018a+ yet because of changes made the the MEX API.

## Usage
First pick up the latest [release](https://github.com/gharveymn/matshare/releases). You may need to compile `matshare` if you are running Linux, in which case just navigate to `matshare_source` and run `INSTALL.m`. 

Here's a very simple example of how to use `matshare`:

#### Process 1
```matlab
>> [s1,s2,s3] = matshare.share({7,uint32(2)},5,rand(3))
s1 = 
matshare object storing cell
    data: {[7]  [2]}
s2 = 
matshare object storing double
    data: 5
s3 = 
matshare object storing double
    data: [3×3 double]
```
#### Process 2
```matlab
>> f = matshare.fetch
f = 
  struct with fields:

    recent: [1×1 matshare.object]
       new: [1×3 matshare.object]
       all: [1×3 matshare.object]
>> disp(f.recent.data)
         0.814723686393179         0.913375856139019         0.278498218867048
         0.905791937075619          0.63235924622541         0.546881519204984
         0.126986816293506        0.0975404049994095         0.957506835434298
```

For more details, you can navigate through the documentation with `help matshare` or `help matshare.[command]`, replacing `[command]` with any one of the `matshare` commands.

## Future Development
This is not a finished project by any means. If you wish to contribute or have any suggestions feel free to contact me. Currently I plan to implement the following at some point in the future:
- Variable identifiers (being able to select shared variables by name)
- Atomic variable operations (increments, decrements, swaps, etc.)
- Variable overwriting by index
- Thread locks on a sub-global level
- Rewrite in C++ for R2018a+
- User defined locks