%% TESTSUITE
%  Runs an automated suite of tests

%% Parallel pool startup
matshare.scripts.poolstartup;
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

fprintf('Test suite ran successfully.\n\n');









