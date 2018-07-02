%% TESTSUITE
%  Runs an automated suite of tests

addpath(fullfile(fileparts(which(mfilename)),'tests'));
addpath(fullfile(fileparts(which(mfilename)),'tests', 'parallel'));
addpath(fullfile(fileparts(which(mfilename)),'scripts'));

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









