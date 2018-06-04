# matshare
`matshare` is a function for process shared memory in MATLAB. This is partially based on the work done by Joshua Dillon for [`sharedmatrix`](https://www.mathworks.com/matlabcentral/fileexchange/28572-sharedmatrix) (which was broken by a recent release), and my main goals here were to provide a higher amount of robustness and to make working with shared memory as simple as possible. Among the features of `matshare` are automatic initialization, which allows one to omit explicit initialization and the worry of double initialization, and thread safety among different processes as well as threads created inside a process. However, one of the restrictions of `matshare` is that the function allows for exactly one variable in shared memory at a time, although this is easy to resolve since structs and cell arrays may be shared.

## Usage
To compile `matshare` simply run INSTALL.m. Run ‘./tests/testmulti.m’ to verify that everything is working correctly. There are several ways to use `matshare` depending on your needs, and some methods give better performance than others. In the interest of performance, both the automatic initialization and thread safety features may be disabled by editing OPTIONS.m (thread safety can also be turned off after compilation using `mshconfig`).


 The easiest way to use `matshare` is with objects. The class MatShare.m allows one to treat shared variables as if they were normal variables. To use this, create an object and use the ‘s_data’ property to read and write to shared memory. For example,

#### Process 1
```matlab
>> shared1 = MatShare;
>> shared1.s_data = rand(3);
```
#### Process 2
```matlab
>> shared2 = MatShare;
>> shared2.s_data
ans =
    0.9649    0.9572    0.1419
    0.1576    0.4854    0.4218
    0.9706    0.8003    0.9157
```

If you assign a new value to the `s_data` property `MatShare` will update the shared memory automatically, and also clean up any existing references you might have. Multiple objects in the same process will point to the same memory, and the memory is only cleared when the last object in the last process is cleared. The penalty with assuming this syntax is mostly performance; since this is using a  MATLAB class it is natural that the performance is slower.


A faster method of using `matshare` is with the entry functions prefixed with ‘`msh`’. The syntax in this case is also very simple, 

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

`matshare` will automatically free the previously written variable and clean up its references if you use mshshare again. If you want to clear the memory used by the current process, use the function mshdetach (again, `matshare` will only actually free the shared memory when every process has called mshdetach).


If you wish for even better performance you may disable the automatic initialization feature. The syntax in this case is, 

#### Process 1
```matlab
>> mshinit
>> mshshare(rand(3));
```
#### Process 2
```matlab
>> mshinit
>> mshfetch
ans =
    0.9649    0.9572    0.1419
    0.1576    0.4854    0.4218
    0.9706    0.8003    0.9157
>> mshdetach
```
Note that you must call `mshdetach` before calling `mshinit` again in the same process, otherwise MATLAB will crash.


Another feature of `matshare` is the ability to set access rights to the shared memory. So if you have another user on the computer who wants access to a large variable, you can do with `mshconfig` (available only on Linux). Default access rights are set to 0600, but one can change this with 
```matlab
>> mshconfig(‘Security’, ’0777’);
```
Note that this will reset after calling mshdetach in all processes. You may also use this function to turn off the thread safety features temporarily with 
```matlab
>> mshconfig(‘ThreadSafety’, ’disable’);
```