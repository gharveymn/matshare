%% TESTSUITE
%  Runs an automated suite of tests

addpath('..');

%% Parallel pool startup
[numworkers, workerpids] = matshare.utils.poolstartup;
observerpid = workerpids{1};

% test essential variables
matshare.tests.parallel.essential;

% test parallel results
matshare.tests.parallel.resultverify;

% test random function calls in parallel
matshare.tests.parallel.randfunccalls;

% parallel function calls
matshare.tests.parallel.lock;

% test parallel results
matshare.tests.parallel.overwrite;

% test subscripted overwriting
matshare.tests.single.subsover;

% test variable operations
matshare.tests.single.varops;

fprintf('Test suite ran successfully.\n\n');









