%% define result verification test parameters

rns = RandStream('mt19937ar');

% max depth of structs and cells
num_maxDepth_tests = 1;
min_maxDepth = 1;
max_maxDepth = 3;
maxDepthV = round(linspace(min_maxDepth, max_maxDepth, num_maxDepth_tests));

% max number of elements
num_maxElements_tests = 2;
min_maxElements = 0;
max_maxElements = 2000;
maxElementsV = round(linspace(min_maxElements, max_maxElements, num_maxElements_tests));

% max number of dimensions
num_maxDims_tests = 2;
min_maxDims = 2;
max_maxDims = 1000;
maxDimsV = round(linspace(min_maxDims, max_maxDims, num_maxDims_tests));

% max children per level in structs and cells
num_maxChildren_tests = 2;
min_maxChildren = 0;
max_maxChildren = 10;
maxChildrenV = round(linspace(min_maxChildren, max_maxChildren, num_maxChildren_tests));

% zero means randvargen can decide
num_typespec_tests = 1;
min_typespec = 0;
max_typespec = 0;
typespecV = linspace(min_typespec, max_typespec, num_typespec_tests);

% number of samples per variable generation model
num_samples = 10;