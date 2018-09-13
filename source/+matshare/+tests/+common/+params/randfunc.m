rns = RandStream('mt19937ar', 'Seed', mod(now*10^10, feature('getpid')));

%% Define random call test parameters

% bound of clearing of data
bounds.clear_data = 50;

bounds.clear_new = 50;

bounds.clear_all = 50;

% bound of random call to clearshm
bounds.clearshm = 200;

bounds.mshdetach = 200;

% bound of random call to mshconfig to set to copy-on-write
bounds.chpar_copy = 50;

% bound of random call to mshconfig to set to overwrite
bounds.chpar_over = 50;

% bound of random call to mshconfig to set gc on
bounds.chpar_gc_on = 30;

% bound of random call of mshconfig to set gc off
bounds.chpar_gc_off = 30;

% max depth of structs and cells
num_maxDepth_tests = 1;
min_maxDepth = 2;
max_maxDepth = 2;
maxDepthV = round(linspace(min_maxDepth, max_maxDepth, num_maxDepth_tests));

% max number of elements
num_maxElements_tests = 2;
min_maxElements = 0;
max_maxElements = 5000;
maxElementsV = round(linspace(min_maxElements, max_maxElements, num_maxElements_tests));

% max number of dimensions
num_maxDims_tests = 2;
min_maxDims = 2;
max_maxDims = 7;
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
num_samples = 1000;

randdoubles1 = rand(rns,[num_samples,1]);
randdoubles2 = rand(rns,[num_samples,1]);
randdoubles3 = rand(rns,[num_samples,1]);