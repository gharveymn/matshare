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

% parallel function calls
testparrandfunccalls;

mshparam('gc', 'on', 'sharetype', 'copy');

% test essential variables
testparessential;

% test parallel results
testparresultverify;

% parallel function calls
testparrandfunccalls;

fprintf('Test suite ran successfully.\n\n');









