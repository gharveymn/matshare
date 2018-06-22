addpath(fullfile(mfilefolder(mfilename), '..', 'vargen'));

%% Define random call test parameters

% bound of clearing of data
bounds.clear_data = 50;

bounds.clear_new = 50;

bounds.clear_all = 50;

% bound of running mshreset
bounds.mshreset = 200;

% bound of running mshlocalcopy
bounds.mshlocalcopy = 10;

% bound of random call to mshclear
bounds.mshclear = 200;

% bound of random call to mshdetach
bounds.mshdetach = 5;

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

count = 1;
total_num_tests = num_maxDepth_tests*num_maxElements_tests*num_maxDims_tests*num_maxChildren_tests*num_typespec_tests;

%% random call combination test
lents = 0;
fprintf('Running tests with random combinations of commands...\n');
for i = 1:num_maxDepth_tests
	maxDepth = maxDepthV(i);
	for j = 1:num_maxElements_tests
		maxElements = maxElementsV(j);
		for k = 1:num_maxDims_tests
			maxDims = maxDimsV(k);
			for l = 1:num_maxChildren_tests
				maxChildren = maxChildrenV(l);
				for m = 1:num_typespec_tests
					typespec = typespecV(m);
					timestr = sprintf(['Overall test    %d of %d\n'...
					                   'Depth test      %d of %d (maxDepth = %d)\n'...
								    'Elements test   %d of %d (maxElements = %d)\n'...
								    'Dimensions test %d of %d (maxDimensions = %d)\n'...
								    'Children test   %d of %d (maxChildren = %d)\n'...
								    'Typespec test   %d of %d (typespec = %d)\n'],...
								    count, total_num_tests,...
								    i,num_maxDepth_tests,maxDepth,...
								    j,num_maxElements_tests,maxElements,...
								    k,num_maxDims_tests,maxDims,...
								    l,num_maxChildren_tests,maxChildren,...
								    m,num_typespec_tests,typespec);
					fprintf([repmat('\b',1,lents) timestr]);
					lents = numel(timestr);
					parfor o = 1:numworkers
						testobjrandfuncworker(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, observerpid, true);
					end
					count = count + 1;
				end
			end
		end
	end
end
matshare.clear
matshare.debug
matshare.detach
fprintf('Test successful.\n\n');