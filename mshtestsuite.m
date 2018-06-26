%% TESTSUITE
%  Runs an automated suite of tests

addpath('tests');
addpath('tests/parallel');
addpath('scripts');

%% Parallel pool startup
mshpoolstartup;
observerpid = workerpids{1};

% test essential variables
testparessential;

% test parallel results
testparresultverify;

% test random function calls in parallel
testparrandfunccalls;

% parallel function calls
testparlock;

% test parallel results
testparoverwrite;

fprintf('Test suite ran successfully.\n\n');









