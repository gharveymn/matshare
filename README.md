# matshare
system for sharing memory between sessions of MATLAB

## Goals for 1.0
Simple sharing using shm_open in persistent memory with pointers to constituents in a hashtable. Users will need to initialize and ensure that data is placed in shared memory only once. Variables can then be retrieved, after which the user will manually shut down. This is at the proof of concept stage, so eventually there may be ability to dynamically change variables, implement locks, and get rid of the manual startup/shutdown.

## Syntax (subject to change)
To initialize
```Matlab
	matshare('open');
```
To place a variable into shared memory
```Matlab
	v = rand(50);
	matshare('share',v);
```
To retrieve a variable from shared memory
```Matlab
	v = matshare('get','v');
```
To shut down
```Matlab
	matshare('close');
```

TODO: Implement this as a MATLAB class to automatically handle setting of variables, so that shared variables are
mutable like any other MATLAB variable.