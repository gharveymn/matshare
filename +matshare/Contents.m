%% MATSHARE  A subsystem for shared memory in MATLAB.
%    Compatibility: R2008b-R2017b
%
%    MATSHARE is a subsystem for MATLAB which provides functions for 
%    creating and operating on shared memory. It has mechanisms for 
%    automatic garbage collection, thread safe memory creation, and 
%    in-place overwriting.
%
%    To access MATSHARE functions add this folder to your path and call 
%    with the syntax <strong>matshare.[command]</strong>.
%
%    List of functions:
%        Primary:
%            matshare.share           - Copy a variable to shared memory
%            matshare.fetch           - Fetch variables from shared memory 
%            matshare.clearshm        - Clear variables from shared memory
%            matshare.detach          - Detach shared memory from this process
%        Utility:            
%            matshare.config          - Configure MATSHARE
%            matshare.status          - Print the current status of MATSHARE
%            matshare.copy            - Copy a variable from shared memory
%            matshare.debug           - Print out MATSHARE debug information
%            matshare.mshreset        - Reset the configuration and clear all variables from shared memory
%            matshare.lock            - Acquire the MATSHARE interprocess lock
%            matshare.unlock          - Release the MATSHARE interprocess lock
%            matshare.clean           - Run MATSHARE garbge collection
%
%    MATSHARE stores the shared memory in <a href="matlab:help matshare.object">matshare objects</a>. You can access
%    the shared memory by querying the <a href="matlab:help matshare.object/data">data</a> property. Methods available to 
%    <a href="matlab:help matshare.object">matshare objects</a> are as follows:
%        matshare.object.overwrite    - Overwrite the variable in-place
%        matshare.object.copy         - Copy the variable from shared memory
%        matshare.object.clear        - Clear the variable from shared memory

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.