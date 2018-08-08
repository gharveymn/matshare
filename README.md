# `matshare`

`matshare` is a subsystem for MATLAB R2008b to R2017b which provides functions for creating and operating on shared memory. It has mechanisms for automatic garbage collection, thread-safe memory creation, and in-place overwriting.

## Availability
MATLAB R2008b to R2017b on Windows and Linux. This is not available for R2018a+ yet because of changes made the the MEX API. This is also not available for MacOS partially because of problems with locking files but mainly because I don't own a Mac.

## Usage
First pick up the latest [release](https://github.com/gharveymn/matshare/releases). You may need to compile `matshare` if you are running Linux, in which case just navigate to `source` and run `INSTALL.m`. 


### Simple Example

##### Process 1

<pre>
>> matshare.share(rand(3));
</pre>

##### Process 2

<pre>
>> fetched = matshare.fetch
fetched = 
  <font color="blue"><u>matshare object</u></font> storing <strong>3x3</strong> <font color="blue"><u>double</u></font>:
    data: [3×3 double]
>> fetched.data
ans =
         0.814723686393179         0.913375856139019         0.278498218867048
         0.905791937075619          0.63235924622541         0.546881519204984
         0.126986816293506        0.0975404049994095         0.957506835434298    
</pre>

### Named Variables and Overwriting

##### Process 1

<pre>
>> shared = matshare.share('-n', 'myvarname', 1);
>> shared.data
ans =
     1
</pre>

##### Process 2

<pre>
>> fetched = matshare.fetch('myvarname')
fetched = 
  <font color="blue"><u>matshare object</u></font> storing <strong>1x1</strong> <font color="blue"><u>double</u></font>:
    data: 1
>> fetched.overwrite(2);    
</pre>

##### Process 1
<pre>
>> shared.data
ans =
     2
</pre>

For more details, you can navigate through the documentation with `help matshare` or `help matshare.[command]`, replacing `[command]` with any one of the `matshare` commands.

## Future Development
This is not a finished project by any means. If you wish to contribute or have any suggestions feel free to contact me. Currently I plan to implement the following at some point in the future:
- Atomic variable operations (increments, decrements, swaps, etc.)
- Variable overwriting by index
- Thread locks on a sub-global level
- Rewrite in C++ for R2018a+
- User defined locks/waits
- Direct usage of matshare objects without the 'data' property