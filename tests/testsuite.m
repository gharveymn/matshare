%% TESTSUITE
%  Runs an automated suite of tests

%% Type classes
mxUNKNOWN_CLASS  = 0;
mxCELL_CLASS     = 1;
mxSTRUCT_CLASS   = 2;
mxLOGICAL_CLASS  = 3;
mxCHAR_CLASS     = 4;
mxVOID_CLASS     = 5;
mxDOUBLE_CLASS   = 6;
mxSINGLE_CLASS   = 7;
mxINT8_CLASS     = 8;
mxUINT8_CLASS    = 9;
mxINT16_CLASS    = 10;
mxUINT16_CLASS   = 11;
mxINT32_CLASS    = 12;
mxUINT32_CLASS   = 13;
mxINT64_CLASS    = 14;
mxUINT64_CLASS   = 15;
mxFUNCTION_CLASS = 16;
mxOPAQUE_CLASS   = 17;
mxOBJECT_CLASS   = 18;

%% Parallel pool startup
poolstartup;
observerpid = workerpids{1};

% test essential variables
testparessential;

% test parallel results
testparresultverify;

% parallel function calls
testparrandfunccalls;

% test again
testparessential;

fprintf('Test successful.\n\n');









