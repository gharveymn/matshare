# matshare

## Usage
To compile `matshare` run INSTALL.m. Run `mstestsuite.m` to verify that everything is working correctly. There are several ways to use `matshare` depending on your needs, and some methods give better performance than others.

The main method of using `matshare` is with the entry functions prefixed with `msh`. The syntax in this case is simple, 

#### Process 1
```matlab
>> mshshare(rand(3));
```
#### Process 2
```matlab
>> mshfetch
ans =
    0.9649    0.9572    0.1419
    0.1576    0.4854    0.4218
    0.9706    0.8003    0.9157
```

Calling `mshclear` will clean up all references in MATLAB and remove the shared memory.

Another feature of `matshare` is the ability to set access rights to the shared memory. So if you have another user on the computer who wants access to a large variable, you can do with `mshconfig` (available only on Linux). Default access rights are set to 0600, but one can change this with 
```matlab
>> mshconfig(‘Security’, ’0777’);
```
Note that this will reset after calling mshdetach in all processes. You may also use this function to turn off the thread safety features temporarily with 
```matlab
>> mshconfig(‘ThreadSafety’, ’off’);
```

## Future Development
- Variable identifiers (being able to select shared variables by name)
- Atomic variable operations (increments, decrements, swaps, etc.)
- Variable overwriting by index
- Thread locks on a sub-global level
- Rewrite in C++ for R2018a+
- User defined locks